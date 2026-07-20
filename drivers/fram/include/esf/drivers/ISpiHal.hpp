#pragma once

#include <cstddef>
#include <cstdint>

namespace esf::drivers {

/**
 * @brief SPI HAL abstraction for the FRAM driver.
 *
 * The FramStorageDriver does not link against any STM32 HAL directly.
 * Instead it depends on this pure interface, which the application injects
 * at construction time.  This keeps the framework hardware-independent and
 * trivially testable on a host machine.
 *
 * Concrete implementations wrap, for example, HAL_SPI_Transmit /
 * HAL_SPI_Receive from STM32 CubeHAL.
 */
class ISpiHal {
public:
    virtual ~ISpiHal() = default;

    /**
     * @brief Assert the chip-select line (active low).
     */
    virtual void csAssert() = 0;

    /**
     * @brief Deassert the chip-select line.
     */
    virtual void csDeassert() = 0;

    /**
     * @brief Transmit @p length bytes from @p data over SPI.
     * @return true on success.
     */
    virtual bool transmit(const uint8_t* data, size_t length) = 0;

    /**
     * @brief Receive @p length bytes into @p data over SPI.
     * @return true on success.
     */
    virtual bool receive(uint8_t* data, size_t length) = 0;
};

} // namespace esf::drivers
