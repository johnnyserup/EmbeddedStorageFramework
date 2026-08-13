#include "esf/Crc32.hpp"

namespace esf {

uint32_t Crc32::compute(const uint8_t* data,
                         uint32_t       length,
                         uint32_t       init) noexcept {
    uint32_t crc = init;

    while (length-- > 0u) {
        crc ^= static_cast<uint32_t>(*data++);
        for (int i = 0; i < 8; ++i) {
            if ((crc & 1u) != 0u) {
                crc = (crc >> 1u) ^ kPolynomial;
            } else {
                crc >>= 1u;
            }
        }
    }

    return crc ^ 0xFFFFFFFFu;
}

} // namespace esf
