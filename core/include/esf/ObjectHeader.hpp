#pragma once

#include <cstdint>

namespace esf {

/**
 * @brief Metadata header stored in front of every persistent object slot.
 *
 * The layout in memory is:
 *   [ ObjectHeader ][ raw bytes of T ]
 *
 * Fields are stored in the target endianness (little-endian for STM32).
 */
struct ObjectHeader {
    static constexpr uint16_t kMagic = 0xCAFE;

    uint16_t magic;     ///< Must equal kMagic for a valid slot
    uint16_t version;   ///< Data version — used for migration
    uint32_t dataSize;  ///< sizeof(T) at the time of the write
    uint32_t crc;       ///< CRC-32 of the raw T bytes

    /**
     * @brief Return true when the header looks structurally valid.
     *
     * This only checks the magic and size fields — CRC validation is a
     * separate step so that callers can distinguish "empty slot" from
     * "corrupted data".
     */
    [[nodiscard]] constexpr bool isValid(uint32_t expectedDataSize) const noexcept {
        return magic == kMagic && dataSize == expectedDataSize;
    }
};

static_assert(sizeof(ObjectHeader) == 12,
              "ObjectHeader layout changed — update flash image migration notes");

} // namespace esf
