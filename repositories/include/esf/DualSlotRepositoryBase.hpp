#pragma once

#include "esf/IRepository.hpp"
#include "esf/Crc32.hpp"
#include "esf/IStorageDriver.hpp"
#include "esf/ObjectHeader.hpp"
#include "esf/StorageObject.hpp"

#include <array>
#include <concepts>
#include <cstdint>
#include <limits>

namespace esf {

/**
 * @brief CRTP base that implements load/save/reset with a two-slot
 *        (ping-pong) commit scheme for power-loss resilience.
 *
 * The two-slot scheme guarantees that at least one previously valid image
 * is always recoverable after an interrupted write.
 *
 * ## On-storage layout per slot
 *
 * Each slot occupies `StorageObject<T,Ver,ObjectId>::kTotalSize` bytes with
 * one modification: the `ObjectHeader::reserved` field carries a **16-bit
 * wrapping generation counter** instead of the usual zero.
 *
 * ```
 * [ ObjectHeader (16 bytes, reserved = generation) ][ T (sizeof(T) bytes) ]
 * ```
 *
 * Two such slots are stored back-to-back in the driver:
 *
 * ```
 * [ Slot A at storageAddress() ][ Slot B at storageAddress() + kSlotSize ]
 * ```
 *
 * The slot with the higher valid generation (using wrapping comparison) is
 * the active slot.  The other slot is the standby slot.
 *
 * ## Write protocol
 *
 * 1. Determine which slot is currently active (A or B).
 * 2. Write the new data to the *standby* slot with generation = active.generation + 1.
 * 3. The write is atomic from the reader's perspective: if power is lost
 *    mid-write the standby slot has an invalid CRC and the previously active
 *    slot is still valid and readable.
 *
 * ## Derived class contract
 *
 * Derived classes supply:
 *  - `storageAddress()` — byte offset of **Slot A** in the driver.
 *  - `defaultValue()`   — factory-default T returned when neither slot is valid.
 *  - Optionally `migrate(oldVersion, payloadBytes, payloadSize, out)` for
 *    version upgrades, identical to `RepositoryBase`.
 *
 * The total storage footprint is `2 * kSlotSize` bytes starting at
 * `storageAddress()`.
 *
 * @tparam Derived   Concrete repository class (CRTP).
 * @tparam T         Data type to persist (trivially copyable, standard layout).
 * @tparam Ver       Object version for migration.
 * @tparam ObjectId  Stable logical identifier stored in the slot header.
 */
template <typename Derived, typename T, uint16_t Ver = 1, uint16_t ObjectId = 0u>
    requires std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>
class DualSlotRepositoryBase : public IRepository<T> {
public:
    using Object = StorageObject<T, Ver, ObjectId>;

    /// Size of a single slot in bytes.
    static constexpr uint32_t kSlotSize = Object::kTotalSize;

    /// Total storage footprint (two slots).
    static constexpr uint32_t kTotalStorageSize = 2u * kSlotSize;

    explicit DualSlotRepositoryBase(IStorageDriver& driver) noexcept
        : driver_(driver) {}

    // -----------------------------------------------------------------------
    // IRepository<T>
    // -----------------------------------------------------------------------

    StorageError load(T& out) override {
        SlotInfo a{};
        SlotInfo b{};
        readSlotInfo(slotAddress(0u), a);
        readSlotInfo(slotAddress(1u), b);

        const bool aValid = a.valid;
        const bool bValid = b.valid;

        if (!aValid && !bValid) {
            out = derived().defaultValue();
            return StorageError::InvalidMagic;
        }

        // Pick the slot with the higher generation (wrapping comparison).
        const bool useB = bValid && (!aValid || generationIsNewer(b.generation, a.generation));
        const uint32_t activeAddr = useB ? slotAddress(1u) : slotAddress(0u);
        const SlotInfo& active    = useB ? b : a;

        if (active.version != Ver) {
            return loadMigrated(activeAddr, active, out);
        }

        return loadPayload(activeAddr, active, out);
    }

    StorageError save(const T& value) override {
        // Read current active slot to get its generation.
        SlotInfo a{};
        SlotInfo b{};
        readSlotInfo(slotAddress(0u), a);
        readSlotInfo(slotAddress(1u), b);

        const bool aValid = a.valid;
        const bool bValid = b.valid;

        uint16_t nextGen = 1u; // default: first write ever
        uint8_t  writeSlot = 0u;

        if (!aValid && !bValid) {
            // Neither slot valid — write to slot A with generation 1.
            writeSlot = 0u;
            nextGen   = 1u;
        } else if (aValid && !bValid) {
            // A is active — write to B with generation = a.generation + 1.
            writeSlot = 1u;
            nextGen   = static_cast<uint16_t>(a.generation + 1u);
        } else if (!aValid && bValid) {
            // B is active — write to A with generation = b.generation + 1.
            writeSlot = 0u;
            nextGen   = static_cast<uint16_t>(b.generation + 1u);
        } else {
            // Both valid — active is the newer one; write to the other.
            if (generationIsNewer(b.generation, a.generation)) {
                // B is active, write to A.
                writeSlot = 0u;
                nextGen   = static_cast<uint16_t>(b.generation + 1u);
            } else {
                // A is active, write to B.
                writeSlot = 1u;
                nextGen   = static_cast<uint16_t>(a.generation + 1u);
            }
        }

        return writeSlotWithGeneration(slotAddress(writeSlot), value, nextGen);
    }

    StorageError reset() override {
        return save(derived().defaultValue());
    }

protected:
    IStorageDriver& driver_;

    /**
     * @brief Default migration handler — returns VersionMismatch.
     *
     * Same contract as RepositoryBase::migrate().
     */
    StorageError migrate(uint16_t /*oldVersion*/,
                         const uint8_t* /*payloadBytes*/,
                         uint32_t /*payloadSize*/,
                         T& out) {
        out = derived().defaultValue();
        return StorageError::VersionMismatch;
    }

private:
    // -----------------------------------------------------------------------
    // Internal slot metadata
    // -----------------------------------------------------------------------

    struct SlotInfo {
        bool     valid      = false;
        uint16_t generation = 0u;
        uint16_t version    = 0u;
        uint32_t dataSize   = 0u;
        uint32_t crcInHeader = 0u;
    };

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    [[nodiscard]] uint32_t slotAddress(uint8_t slotIndex) const noexcept {
        return derived().storageAddress() +
               static_cast<uint32_t>(slotIndex) * kSlotSize;
    }

    /**
     * @brief Read the ObjectHeader from @p addr and populate @p info.
     *
     * A slot is considered valid when its header magic and objectId match
     * AND the CRC over the payload bytes matches the stored CRC.  The
     * `reserved` field carries the generation counter and is not checked
     * for zero (unlike the single-slot scheme).
     */
    void readSlotInfo(uint32_t addr, SlotInfo& info) {
        ObjectHeader hdr{};
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        if (!driver_.read(addr, reinterpret_cast<uint8_t*>(&hdr), sizeof(ObjectHeader))) {
            return;
        }

        // Check magic and objectId only (not reserved — it holds the generation).
        if (hdr.magic != ObjectHeader::kMagic || hdr.objectId != ObjectId) {
            return;
        }

        // Bounds check.
        const uint32_t payloadAddr = addr + static_cast<uint32_t>(sizeof(ObjectHeader));
        if (!payloadFitsInStorage(payloadAddr, hdr.dataSize)) {
            return;
        }

        // Read payload and validate CRC.
        std::array<uint8_t, kMaxPayloadForCrcCheck> payloadBuf{};
        if (hdr.dataSize > payloadBuf.size()) {
            // Payload too large for our stack buffer — cannot validate.
            return;
        }

        if (!driver_.read(payloadAddr, payloadBuf.data(), hdr.dataSize)) {
            return;
        }

        if (hdr.crc != Crc32::compute(payloadBuf.data(), hdr.dataSize)) {
            return;
        }

        info.valid       = true;
        info.generation  = hdr.reserved; // generation stored in reserved field
        info.version     = hdr.version;
        info.dataSize    = hdr.dataSize;
        info.crcInHeader = hdr.crc;
    }

    /**
     * @brief Return true when @p genA is strictly newer than @p genB using
     *        wrapping (half-range) comparison.
     */
    [[nodiscard]] static bool generationIsNewer(uint16_t genA, uint16_t genB) noexcept {
        // Wrapping comparison: A is newer if (A - B) mod 2^16 < 2^15.
        return static_cast<uint16_t>(genA - genB) <
               static_cast<uint16_t>(std::numeric_limits<uint16_t>::max() / 2u + 1u);
    }

    /**
     * @brief Load the current-version payload from an active slot.
     */
    StorageError loadPayload(uint32_t slotAddr, const SlotInfo& info, T& out) {
        if (info.dataSize != sizeof(T)) {
            out = derived().defaultValue();
            return StorageError::InvalidMagic;
        }

        const uint32_t payloadAddr = slotAddr + static_cast<uint32_t>(sizeof(ObjectHeader));
        Object obj{};
        // Reconstruct header so hasValidCrc() can compare.
        obj.header.magic    = ObjectHeader::kMagic;
        obj.header.objectId = ObjectId;
        obj.header.version  = Ver;
        obj.header.reserved = info.generation;
        obj.header.dataSize = info.dataSize;
        obj.header.crc      = info.crcInHeader;

        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        if (!driver_.read(payloadAddr,
                          reinterpret_cast<uint8_t*>(&obj.data),
                          sizeof(T))) {
            return StorageError::DriverError;
        }

        if (!obj.hasValidCrc()) {
            out = derived().defaultValue();
            return StorageError::CrcMismatch;
        }

        out = obj.data;
        return StorageError::Ok;
    }

    /**
     * @brief Load and migrate an older-version payload from @p activeAddr.
     */
    StorageError loadMigrated(uint32_t activeAddr, const SlotInfo& info, T& out) {
        (void)info; // info already confirmed valid with mismatched version

        ObjectHeader hdr{};
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        if (!driver_.read(activeAddr, reinterpret_cast<uint8_t*>(&hdr), sizeof(ObjectHeader))) {
            return StorageError::DriverError;
        }

        static constexpr uint32_t kBufSize = kEffectiveMigrationBufferSize;
        std::array<uint8_t, kBufSize> payload{};
        if (hdr.dataSize > payload.size()) {
            out = derived().defaultValue();
            return StorageError::VersionMismatch;
        }

        const uint32_t payloadAddr = activeAddr + static_cast<uint32_t>(sizeof(ObjectHeader));
        if (!driver_.read(payloadAddr, payload.data(), hdr.dataSize)) {
            return StorageError::DriverError;
        }

        if (hdr.crc != Crc32::compute(payload.data(), hdr.dataSize)) {
            out = derived().defaultValue();
            return StorageError::CrcMismatch;
        }

        return derived().migrate(hdr.version, payload.data(), hdr.dataSize, out);
    }

    /**
     * @brief Write @p value to @p addr using the two-slot header format
     *        (generation stored in `ObjectHeader::reserved`).
     */
    StorageError writeSlotWithGeneration(uint32_t addr,
                                         const T& value,
                                         uint16_t generation) {
        Object obj = Object::make(value);
        // Store generation in the reserved field (dual-slot convention).
        obj.header.reserved = generation;

        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        if (!driver_.write(addr,
                           reinterpret_cast<const uint8_t*>(&obj),
                           Object::kTotalSize)) {
            return StorageError::DriverError;
        }

        return StorageError::Ok;
    }

    [[nodiscard]] bool payloadFitsInStorage(uint32_t payloadAddress,
                                            uint32_t payloadSize) const noexcept {
        const auto cap = static_cast<uint32_t>(driver_.capacity());
        return payloadAddress <= cap && payloadSize <= (cap - payloadAddress);
    }

    [[nodiscard]] Derived& derived() noexcept {
        return static_cast<Derived&>(*this);
    }
    [[nodiscard]] const Derived& derived() const noexcept {
        return static_cast<const Derived&>(*this);
    }

    // Maximum payload size we will read onto the stack for CRC validation.
    // In readSlotInfo we use this to guard the stack buffer.
    static constexpr uint32_t kMaxPayloadForCrcCheck = []() constexpr {
        if constexpr (requires { Derived::kMigrationBufferSize; }) {
            return static_cast<uint32_t>(
                (sizeof(T) > Derived::kMigrationBufferSize) ? sizeof(T)
                                                            : Derived::kMigrationBufferSize);
        } else {
            return static_cast<uint32_t>(sizeof(T));
        }
    }();

    static constexpr uint32_t kEffectiveMigrationBufferSize = []() constexpr {
        if constexpr (requires { Derived::kMigrationBufferSize; }) {
            return static_cast<uint32_t>(Derived::kMigrationBufferSize);
        } else {
            return static_cast<uint32_t>(sizeof(T));
        }
    }();
};

} // namespace esf
