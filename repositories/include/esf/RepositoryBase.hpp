#pragma once

#include "esf/IRepository.hpp"
#include "esf/Crc32.hpp"
#include "esf/IStorageDriver.hpp"
#include "esf/StorageObject.hpp"

#include <array>
#include <concepts>

namespace esf {

/**
 * @brief CRTP-friendly base class that implements the full load/save/reset
 *        protocol on top of an IStorageDriver.
 *
 * Derived classes only need to supply:
 *  - a StorageAddress (byte offset into the driver)
 *  - a stable ObjectId template argument unique within the storage map
 *  - a default value returned when no valid slot is found
 *
 * Usage:
 * @code
 * class SettingsRepository
 *     : public RepositoryBase<SettingsRepository, Settings, 1>
 * {
 *     static constexpr uint32_t kAddress = 0;
 *     static constexpr Settings kDefault = {};
 *     // ... see ExampleSettingsRepository.hpp for a full example
 * };
 * @endcode
 *
 * @tparam Derived   The concrete repository class (CRTP).
 * @tparam T         Data type to persist.
 * @tparam Ver       Object version for migration.
 * @tparam ObjectId  Stable identifier stored in the slot header.
 */
template <typename Derived, typename T, uint16_t Ver = 1, uint16_t ObjectId = 0u>
    requires std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>
class RepositoryBase : public IRepository<T> {
public:
    using Object = StorageObject<T, Ver, ObjectId>;

    explicit RepositoryBase(IStorageDriver& driver) noexcept : driver_(driver) {}

    // -----------------------------------------------------------------------
    // IRepository<T>
    // -----------------------------------------------------------------------

    StorageError load(T& out) override {
        ObjectHeader header{};
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        if (!driver_.read(derived().storageAddress(),
                          reinterpret_cast<uint8_t*>(&header),
                          sizeof(ObjectHeader))) {
            return StorageError::DriverError;
        }

        if (!header.matchesSlot(ObjectId)) {
            out = derived().defaultValue();
            return StorageError::InvalidMagic;
        }

        const auto payloadAddress = derived().storageAddress() +
                                    static_cast<uint32_t>(sizeof(ObjectHeader));
        if (!payloadFitsInStorage(payloadAddress, header.dataSize)) {
            out = derived().defaultValue();
            return StorageError::InvalidMagic;
        }

        if (header.version != Ver) {
            std::array<uint8_t, kEffectiveMigrationBufferSize> payload{};
            if (header.dataSize > payload.size()) {
                out = derived().defaultValue();
                return StorageError::VersionMismatch;
            }

            if (!driver_.read(payloadAddress, payload.data(), header.dataSize)) {
                return StorageError::DriverError;
            }

            if (header.crc != Crc32::compute(payload.data(), header.dataSize)) {
                out = derived().defaultValue();
                return StorageError::CrcMismatch;
            }

            return derived().migrate(header.version, payload.data(), header.dataSize, out);
        }

        if (header.dataSize != sizeof(T)) {
            out = derived().defaultValue();
            return StorageError::InvalidMagic;
        }

        Object obj{};
        obj.header = header;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        if (!driver_.read(payloadAddress,
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

    StorageError save(const T& value) override {
        const Object obj = Object::make(value);

        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        if (!driver_.write(derived().storageAddress(),
                           reinterpret_cast<const uint8_t*>(&obj),
                           Object::kTotalSize)) {
            return StorageError::DriverError;
        }

        return StorageError::Ok;
    }

    StorageError reset() override {
        return save(derived().defaultValue());
    }

protected:
    IStorageDriver& driver_;

    /**
     * @brief Default migration handler — returns VersionMismatch.
     *
     * Derived classes may override this to implement forward/backward
     * migration between object versions without depending on the current
     * in-memory layout of T. The hook is called only after load() has
     * validated the slot header and CRC-checked the raw payload bytes.
     */
    StorageError migrate(uint16_t /*oldVersion*/,
                         const uint8_t* /*payloadBytes*/,
                         uint32_t /*payloadSize*/,
                         T& out) {
        out = derived().defaultValue();
        return StorageError::VersionMismatch;
    }

private:
    static constexpr uint32_t kEffectiveMigrationBufferSize = []() constexpr {
        if constexpr (requires { Derived::kMigrationBufferSize; }) {
            return static_cast<uint32_t>(Derived::kMigrationBufferSize);
        } else {
            return static_cast<uint32_t>(sizeof(T));
        }
    }();

    [[nodiscard]] bool payloadFitsInStorage(uint32_t payloadAddress,
                                            uint32_t payloadSize) const noexcept {
        const auto capacity = static_cast<uint32_t>(driver_.capacity());
        return payloadAddress <= capacity && payloadSize <= (capacity - payloadAddress);
    }

    [[nodiscard]] Derived& derived() noexcept {
        return static_cast<Derived&>(*this);
    }
    [[nodiscard]] const Derived& derived() const noexcept {
        return static_cast<const Derived&>(*this);
    }
};

} // namespace esf
