#pragma once

#include "esf/Crc32.hpp"
#include "esf/ObjectHeader.hpp"

#include <concepts>
#include <cstdint>
#include <cstring>

namespace esf {

/**
 * @brief A versioned, CRC-protected wrapper around a plain data type T.
 *
 * StorageObject<T> is the canonical unit of persistence in the framework.
 * It pairs a strongly-typed data payload with an ObjectHeader so that:
 *  - Corrupted slots are detected via CRC mismatch.
 *  - Stale slots from an older firmware version are detected via the
 *    version field.
 *  - The raw bytes written to / read from storage are contiguous:
 *      [ ObjectHeader (16 bytes) | T (sizeof(T) bytes) ]
 *
 * @tparam T   Plain data type to persist.  Must be trivially copyable and
 *             must not contain pointers (pointers are meaningless after a
 *             power cycle).
 * @tparam Ver Version tag — increment whenever the layout of T changes
 *             in a way that requires a migration step.
 * @tparam ObjectId  Stable logical identity for this persisted object type.
 */
template <typename T, uint16_t Ver = 1, uint16_t ObjectId = 0u>
    requires std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>
struct StorageObject {
    static constexpr uint16_t kVersion  = Ver;
    static constexpr uint16_t kObjectId = ObjectId;
    static constexpr uint32_t kDataSize = static_cast<uint32_t>(sizeof(T));
    static constexpr uint32_t kTotalSize =
        static_cast<uint32_t>(sizeof(ObjectHeader)) + kDataSize;

    ObjectHeader header{};
    T            data{};

    // -----------------------------------------------------------------------
    // Construction helpers
    // -----------------------------------------------------------------------

    /**
     * @brief Create a StorageObject with header populated from @p value.
     *
     * The CRC is computed over the raw bytes of @p value.
     */
    [[nodiscard]] static StorageObject make(const T& value) noexcept {
        StorageObject obj{};
        obj.data            = value;
        obj.header.magic    = ObjectHeader::kMagic;
        obj.header.objectId = kObjectId;
        obj.header.version  = kVersion;
        obj.header.reserved = 0u;
        obj.header.dataSize = kDataSize;
        obj.header.crc      = Crc32::compute(value);
        return obj;
    }

    // -----------------------------------------------------------------------
    // Validation helpers
    // -----------------------------------------------------------------------

    /**
     * @brief Return true when the header magic, object id and size are valid.
     */
    [[nodiscard]] bool hasValidHeader() const noexcept {
        return header.matchesSlot(kObjectId) && header.dataSize == kDataSize;
    }

    /**
     * @brief Return true when the CRC stored in the header matches the
     *        CRC computed over the current data field.
     */
    [[nodiscard]] bool hasValidCrc() const noexcept {
        return header.crc == Crc32::compute(data);
    }

    /**
     * @brief Return true when both the header and CRC are valid.
     */
    [[nodiscard]] bool isValid() const noexcept {
        return hasValidHeader() && hasValidCrc();
    }

    /**
     * @brief Return the stored version number.
     */
    [[nodiscard]] uint16_t version() const noexcept {
        return header.version;
    }
};

} // namespace esf
