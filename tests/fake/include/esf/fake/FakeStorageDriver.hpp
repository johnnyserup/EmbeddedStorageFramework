#pragma once

#include "esf/IStorageDriver.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace esf::fake {

/**
 * @brief Deterministic fake storage driver for unit testing.
 *
 * FakeStorageDriver is functionally identical to RamStorageDriver but lives
 * in the test support tree so that production code can never accidentally
 * depend on it.  It adds extra observability hooks that are useful in tests:
 *
 *  - readCount() / writeCount()  — call-count tracking
 *  - setReadFail() / setWriteFail() — fault injection
 *  - fill() — reset backing memory between test cases
 *
 * @tparam Capacity  Size of the backing store in bytes.
 */
template <size_t Capacity>
class FakeStorageDriver final : public IStorageDriver {
    static_assert(Capacity > 0, "Capacity must be > 0");

public:
    FakeStorageDriver()  = default;
    ~FakeStorageDriver() = default;

    FakeStorageDriver(const FakeStorageDriver&)            = delete;
    FakeStorageDriver& operator=(const FakeStorageDriver&) = delete;
    FakeStorageDriver(FakeStorageDriver&&)                 = delete;
    FakeStorageDriver& operator=(FakeStorageDriver&&)      = delete;

    // -----------------------------------------------------------------------
    // IStorageDriver
    // -----------------------------------------------------------------------

    bool read(uint32_t address, uint8_t* data, size_t length) override {
        ++readCount_;
        if (failOnRead_) {
            return false;
        }
        if (!rangeValid(address, length)) {
            return false;
        }
        for (size_t i = 0; i < length; ++i) {
            data[i] = buffer_[address + i];
        }
        return true;
    }

    bool write(uint32_t address, const uint8_t* data, size_t length) override {
        ++writeCount_;
        if (failOnWrite_) {
            return false;
        }
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

    void fill(uint8_t fillByte = 0xFF) noexcept {
        buffer_.fill(fillByte);
        readCount_  = 0;
        writeCount_ = 0;
    }

    void setReadFail(bool fail)  noexcept { failOnRead_  = fail; }
    void setWriteFail(bool fail) noexcept { failOnWrite_ = fail; }

    [[nodiscard]] unsigned readCount()  const noexcept { return readCount_; }
    [[nodiscard]] unsigned writeCount() const noexcept { return writeCount_; }

    [[nodiscard]] const std::array<uint8_t, Capacity>& raw() const noexcept {
        return buffer_;
    }

private:
    [[nodiscard]] bool rangeValid(uint32_t address, size_t length) const noexcept {
        return (length > 0) && (static_cast<size_t>(address) + length <= Capacity);
    }

    std::array<uint8_t, Capacity> buffer_{};
    unsigned readCount_  = 0;
    unsigned writeCount_ = 0;
    bool     failOnRead_  = false;
    bool     failOnWrite_ = false;
};

} // namespace esf::fake
