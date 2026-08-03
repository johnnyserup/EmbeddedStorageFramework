#pragma once

#include "esf/drivers/ISpiHal.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

namespace esf::fake {

/**
 * @brief Deterministic fake SPI HAL for unit testing the FRAM driver.
 *
 * Observability:
 *  - transmitted        — all bytes sent via transmit(), in order
 *  - receiveQueue       — bytes returned by receive(), consumed FIFO
 *  - csAssertCount      — number of times csAssert() was called
 *  - csDeassertCount    — number of times csDeassert() was called
 *
 * Fault injection:
 *  - failOnTransmit     — transmit() returns false (simulates SPI error)
 *  - failOnReceive      — receive() returns false (simulates SPI error)
 *
 * Call reset() to clear all state between test cases.
 */
class FakeSpiHal final : public drivers::ISpiHal {
public:
    FakeSpiHal()  = default;
    ~FakeSpiHal() = default;

    FakeSpiHal(const FakeSpiHal&)            = delete;
    FakeSpiHal& operator=(const FakeSpiHal&) = delete;
    FakeSpiHal(FakeSpiHal&&)                 = delete;
    FakeSpiHal& operator=(FakeSpiHal&&)      = delete;

    // -----------------------------------------------------------------------
    // ISpiHal
    // -----------------------------------------------------------------------

    void csAssert() override {
        ++csAssertCount;
    }

    void csDeassert() override {
        ++csDeassertCount;
    }

    bool transmit(const uint8_t* data, size_t length) override {
        if (failOnTransmit) {
            return false;
        }
        for (size_t i = 0; i < length; ++i) {
            transmitted.push_back(data[i]);
        }
        return true;
    }

    bool receive(uint8_t* data, size_t length) override {
        if (failOnReceive) {
            return false;
        }
        for (size_t i = 0; i < length; ++i) {
            if (receiveQueue.empty()) {
                data[i] = 0x00u;
            } else {
                data[i] = receiveQueue.front();
                receiveQueue.pop_front();
            }
        }
        return true;
    }

    // -----------------------------------------------------------------------
    // Test helpers
    // -----------------------------------------------------------------------

    /** Reset all state (counts, buffers, fault flags). */
    void reset() {
        transmitted.clear();
        receiveQueue.clear();
        csAssertCount   = 0u;
        csDeassertCount = 0u;
        failOnTransmit  = false;
        failOnReceive   = false;
    }

    std::vector<uint8_t> transmitted;
    std::deque<uint8_t>  receiveQueue;

    unsigned csAssertCount   = 0u;
    unsigned csDeassertCount = 0u;

    bool failOnTransmit = false;
    bool failOnReceive  = false;
};

} // namespace esf::fake
