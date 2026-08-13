#pragma once

#include "esf/IStorageDriver.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace esf::drivers {

/**
 * @brief In-memory storage driver backed by a statically-sized byte array.
 *
 * RamStorageDriver is allocation-free: the backing store is a compile-time
 * fixed-size array embedded directly in the object.  It is useful as:
 *  - A drop-in replacement for real hardware during host-based unit tests.
 *  - A fast scratch-pad in application code that does not need persistence.
 *
 * @tparam Capacity  Size of the backing store in bytes.
 */
template <size_t Capacity>
class RamStorageDriver final : public IStorageDriver {
    static_assert(Capacity > 0, "Capacity must be > 0");

public:
    RamStorageDriver()  = default;
    ~RamStorageDriver() = default;

    // Non-copyable, non-movable (owns a large array)
    RamStorageDriver(const RamStorageDriver&)            = delete;
    RamStorageDriver& operator=(const RamStorageDriver&) = delete;
    RamStorageDriver(RamStorageDriver&&)                 = delete;
    RamStorageDriver& operator=(RamStorageDriver&&)      = delete;

    // -----------------------------------------------------------------------
    // IStorageDriver
    // -----------------------------------------------------------------------

    bool read(uint32_t address, uint8_t* data, size_t length) override {
        if (!rangeValid(address, length)) {
            return false;
        }
        for (size_t i = 0; i < length; ++i) {
            data[i] = buffer_[address + i];
        }
        return true;
    }

    bool write(uint32_t address, const uint8_t* data, size_t length) override {
        if (!rangeValid(address, length)) {
            return false;
        }
        for (size_t i = 0; i < length; ++i) {
            buffer_[address + i] = data[i];
        }
        return true;
    }

    [[nodiscard]] size_t capacity() const override {
        return Capacity;
    }

    // -----------------------------------------------------------------------
    // Test helpers
    // -----------------------------------------------------------------------

    /**
     * @brief Fill the entire backing store with @p fillByte.
     *
     * Useful in tests to reset state between test cases.
     */
    void fill(uint8_t fillByte = 0xFF) noexcept {
        buffer_.fill(fillByte);
    }

    /**
     * @brief Direct read-only access to the raw backing buffer.
     */
    [[nodiscard]] const std::array<uint8_t, Capacity>& raw() const noexcept {
        return buffer_;
    }

private:
    [[nodiscard]] bool rangeValid(uint32_t address, size_t length) const noexcept {
        return (length > 0) && (static_cast<size_t>(address) + length <= Capacity);
    }

    std::array<uint8_t, Capacity> buffer_{};
};

} // namespace esf::drivers
