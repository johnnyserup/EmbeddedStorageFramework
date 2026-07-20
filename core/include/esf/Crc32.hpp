#pragma once

#include <cstdint>

namespace esf {

/**
 * @brief Compile-time and run-time CRC-32 (ISO 3309 / Ethernet polynomial).
 *
 * The implementation is table-free and allocation-free, making it
 * suitable for both host-based unit tests and embedded targets.
 */
class Crc32 {
public:
    static constexpr uint32_t kPolynomial = 0xEDB88320u; ///< Reflected polynomial

    /**
     * @brief Compute CRC-32 over an arbitrary byte buffer.
     *
     * @param data    Pointer to the data buffer.
     * @param length  Number of bytes to process.
     * @param init    Initial CRC value (useful for chaining).
     * @return        Final CRC-32 value.
     */
    [[nodiscard]] static uint32_t compute(const uint8_t* data,
                                          uint32_t       length,
                                          uint32_t       init = 0xFFFFFFFFu) noexcept;

    /**
     * @brief Convenience overload for typed objects.
     *
     * @tparam T  Must be trivially copyable.
     */
    template <typename T>
    [[nodiscard]] static uint32_t compute(const T& value) noexcept {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return compute(reinterpret_cast<const uint8_t*>(&value), sizeof(T));
    }
};

} // namespace esf
