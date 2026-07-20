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
    uint16_t objectId;  ///< Logical object identity within the persistence map
    uint16_t version;   ///< Data version — used for migration
    uint16_t reserved;  ///< Reserved for future flags/alignment, must be zero
    uint32_t dataSize;  ///< sizeof(T) at the time of the write
    uint32_t crc;       ///< CRC-32 of the raw T bytes

    /**
     * @brief Return true when the header looks structurally valid.
     *
     * This only checks the magic and object identifier — payload size and
     * CRC validation are separate steps so that callers can distinguish
     * "wrong slot" from "corrupted data".
     */
    [[nodiscard]] constexpr bool matchesSlot(uint16_t expectedObjectId) const noexcept {
        return magic == kMagic && objectId == expectedObjectId && reserved == 0u;
    }
};

static_assert(sizeof(ObjectHeader) == 16,
              "ObjectHeader layout changed — update flash image migration notes");

} // namespace esf
