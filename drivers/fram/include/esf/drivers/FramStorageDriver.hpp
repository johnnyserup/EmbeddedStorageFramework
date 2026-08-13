#pragma once

#include "esf/IStorageDriver.hpp"
#include "esf/drivers/ISpiHal.hpp"

#include <cstddef>
#include <cstdint>

namespace esf::drivers {

/**
 * @brief Storage driver for SPI FRAM devices (MB85RS family).
 *
 * Implements the byte-level read/write protocol required by the MB85RS64TPN
 * (64 Kbit / 8 KByte SPI FRAM from Fujitsu).  The driver works with any
 * MB85RS device that uses the same opcode set:
 *   - WREN  (0x06) — Write Enable Latch
 *   - READ  (0x03) — Read Memory
 *   - WRITE (0x02) — Write Memory
 *   - RDSR  (0x05) — Read Status Register
 *
 * Hardware details (SPI timing, GPIO, DMA) are entirely encapsulated in the
 * ISpiHal implementation that is injected at construction time.  This driver
 * therefore compiles and links on a host machine without any STM32 headers.
 *
 * @tparam CapacityBytes  Device capacity in bytes (8192 for MB85RS64TPN).
 */
template <size_t CapacityBytes = 8192u>
class FramStorageDriver final : public IStorageDriver {
    static_assert(CapacityBytes > 0,  "CapacityBytes must be > 0");
    static_assert(CapacityBytes <= 0x20000u,
                  "Address encoding only supports up to 17-bit addresses");

public:
    /**
     * @param spi  Reference to the SPI HAL implementation.  Must outlive
     *             this driver.
     */
    explicit FramStorageDriver(ISpiHal& spi) noexcept : spi_(spi) {}

    ~FramStorageDriver() = default;

    FramStorageDriver(const FramStorageDriver&)            = delete;
    FramStorageDriver& operator=(const FramStorageDriver&) = delete;
    FramStorageDriver(FramStorageDriver&&)                 = delete;
    FramStorageDriver& operator=(FramStorageDriver&&)      = delete;

    // -----------------------------------------------------------------------
    // IStorageDriver
    // -----------------------------------------------------------------------

    bool read(uint32_t address, uint8_t* data, size_t length) override {
        if (!rangeValid(address, length)) {
            return false;
        }

        const uint8_t cmd[3] = {
            kOpcodeRead,
            static_cast<uint8_t>((address >> 8u) & 0xFFu),
            static_cast<uint8_t>( address        & 0xFFu),
        };

        spi_.csAssert();
        bool ok = spi_.transmit(cmd, sizeof(cmd));
        if (ok) {
            ok = spi_.receive(data, length);
        }
        spi_.csDeassert();

        return ok;
    }

    bool write(uint32_t address, const uint8_t* data, size_t length) override {
        if (!rangeValid(address, length)) {
            return false;
        }

        // FRAM must be write-enabled before every write transaction.
        if (!writeEnable()) {
            return false;
        }

        const uint8_t cmd[3] = {
            kOpcodeWrite,
            static_cast<uint8_t>((address >> 8u) & 0xFFu),
            static_cast<uint8_t>( address        & 0xFFu),
        };

        spi_.csAssert();
        bool ok = spi_.transmit(cmd, sizeof(cmd));
        if (ok) {
            ok = spi_.transmit(data, length);
        }
        spi_.csDeassert();

        return ok;
    }

    [[nodiscard]] size_t capacity() const override {
        return CapacityBytes;
    }

private:
    // MB85RS opcode set
    static constexpr uint8_t kOpcodeWren  = 0x06u; ///< Write Enable Latch
    static constexpr uint8_t kOpcodeRead  = 0x03u; ///< Read Memory
    static constexpr uint8_t kOpcodeWrite = 0x02u; ///< Write Memory
    static constexpr uint8_t kOpcodeRdsr  = 0x05u; ///< Read Status Register

    [[nodiscard]] bool rangeValid(uint32_t address, size_t length) const noexcept {
        return (length > 0) && (static_cast<size_t>(address) + length <= CapacityBytes);
    }

    /**
     * @brief Send the WREN command — required before every write on MB85RS.
     */
    bool writeEnable() {
        const uint8_t cmd = kOpcodeWren;
        spi_.csAssert();
        const bool ok = spi_.transmit(&cmd, 1u);
        spi_.csDeassert();
        return ok;
    }

    ISpiHal& spi_;
};

} // namespace esf::drivers
