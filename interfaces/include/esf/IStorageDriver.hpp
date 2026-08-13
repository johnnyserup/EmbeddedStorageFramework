#pragma once

#include <cstddef>
#include <cstdint>

namespace esf {

/**
 * @brief Hardware-independent storage driver interface.
 *
 * Concrete implementations provide byte-level read/write access to a
 * specific physical medium (FRAM over SPI, internal RAM buffer, etc.).
 * The application layer never depends on a concrete driver — it always
 * programs against this interface.
 *
 * All addresses are byte-offsets from the beginning of the device.
 * Implementations must be non-blocking where possible and must not
 * perform dynamic memory allocation.
 */
class IStorageDriver {
public:
    virtual ~IStorageDriver() = default;

    /**
     * @brief Read @p length bytes from @p address into @p data.
     * @return true on success, false if the read failed or the range is
     *         out of bounds.
     */
    virtual bool read(uint32_t address, uint8_t* data, size_t length) = 0;

    /**
     * @brief Write @p length bytes from @p data to @p address.
     * @return true on success, false if the write failed or the range is
     *         out of bounds.
     */
    virtual bool write(uint32_t address, const uint8_t* data, size_t length) = 0;

    /**
     * @brief Total capacity of the storage medium in bytes.
     */
    [[nodiscard]] virtual size_t capacity() const = 0;
};

} // namespace esf
