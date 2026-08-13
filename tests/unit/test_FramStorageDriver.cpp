#include "esf/drivers/FramStorageDriver.hpp"
#include "esf/fake/FakeSpiHal.hpp"

#include <gtest/gtest.h>
#include <array>
#include <cstdint>

using esf::drivers::FramStorageDriver;
using esf::fake::FakeSpiHal;

// ---------------------------------------------------------------------------
// Test fixtures
// ---------------------------------------------------------------------------

// Small 256-byte device for most tests.
static constexpr size_t kSmallCap = 256u;
using SmallDriver = FramStorageDriver<kSmallCap>;

// Full 8 KByte device (MB85RS64TPN) for address-encoding tests that need
// addresses above 0x00FF.
static constexpr size_t kFullCap = 8192u;
using FullDriver = FramStorageDriver<kFullCap>;

class FramStorageDriverTest : public ::testing::Test {
protected:
    void SetUp() override { spi.reset(); }

    FakeSpiHal  spi;
    SmallDriver driver{spi};
};

// ---------------------------------------------------------------------------
// ReadProtocol
// ---------------------------------------------------------------------------
TEST_F(FramStorageDriverTest, ReadProtocol) {
    spi.receiveQueue = {0xAAu, 0xBBu, 0xCCu};

    uint8_t buf[3] = {};
    ASSERT_TRUE(driver.read(0x0010u, buf, 3u));

    // Exactly one CS assert/deassert pair.
    EXPECT_EQ(spi.csAssertCount,   1u);
    EXPECT_EQ(spi.csDeassertCount, 1u);

    // Command frame: READ opcode (0x03) + addr_hi + addr_lo.
    ASSERT_EQ(spi.transmitted.size(), 3u);
    EXPECT_EQ(spi.transmitted[0], 0x03u); // READ opcode
    EXPECT_EQ(spi.transmitted[1], 0x00u); // address high byte
    EXPECT_EQ(spi.transmitted[2], 0x10u); // address low byte

    // Received data forwarded to caller.
    EXPECT_EQ(buf[0], 0xAAu);
    EXPECT_EQ(buf[1], 0xBBu);
    EXPECT_EQ(buf[2], 0xCCu);
}

// ---------------------------------------------------------------------------
// WriteProtocol
// ---------------------------------------------------------------------------
TEST_F(FramStorageDriverTest, WriteProtocol) {
    const uint8_t data[] = {0x11u, 0x22u, 0x33u};
    ASSERT_TRUE(driver.write(0x0020u, data, sizeof(data)));

    // Two CS transactions: WREN then WRITE.
    EXPECT_EQ(spi.csAssertCount,   2u);
    EXPECT_EQ(spi.csDeassertCount, 2u);

    // Total transmitted: WREN(1) + WRITE opcode + addr_hi + addr_lo(3) + data(3) = 7.
    ASSERT_EQ(spi.transmitted.size(), 7u);
    EXPECT_EQ(spi.transmitted[0], 0x06u); // WREN opcode
    EXPECT_EQ(spi.transmitted[1], 0x02u); // WRITE opcode
    EXPECT_EQ(spi.transmitted[2], 0x00u); // addr_hi
    EXPECT_EQ(spi.transmitted[3], 0x20u); // addr_lo
    EXPECT_EQ(spi.transmitted[4], 0x11u);
    EXPECT_EQ(spi.transmitted[5], 0x22u);
    EXPECT_EQ(spi.transmitted[6], 0x33u);
}

// ---------------------------------------------------------------------------
// WriteEnablePrecedesData
// ---------------------------------------------------------------------------
TEST_F(FramStorageDriverTest, WriteEnablePrecedesData) {
    const uint8_t data[] = {0xABu};
    ASSERT_TRUE(driver.write(0x0000u, data, 1u));

    // CS sequence: assert→deassert (WREN), assert→deassert (WRITE).
    EXPECT_EQ(spi.csAssertCount,   2u);
    EXPECT_EQ(spi.csDeassertCount, 2u);

    // WREN must be the very first transmitted byte.
    ASSERT_GE(spi.transmitted.size(), 2u);
    EXPECT_EQ(spi.transmitted[0], 0x06u); // WREN in first transaction
    // WRITE opcode must follow in the second transaction.
    EXPECT_EQ(spi.transmitted[1], 0x02u); // WRITE in second transaction
}

// ---------------------------------------------------------------------------
// AddressEncoding — uses the full 8 KByte driver so that all addresses fit.
// ---------------------------------------------------------------------------
class FramAddressEncodingTest : public ::testing::Test {
protected:
    void SetUp() override { spi.reset(); }

    FakeSpiHal spi;
    FullDriver driver{spi};
};

TEST_F(FramAddressEncodingTest, AddressEncoding_0x0000) {
    const uint8_t data[] = {0x00u};
    ASSERT_TRUE(driver.write(0x0000u, data, 1u));
    ASSERT_GE(spi.transmitted.size(), 4u);
    EXPECT_EQ(spi.transmitted[2], 0x00u); // addr_hi
    EXPECT_EQ(spi.transmitted[3], 0x00u); // addr_lo
}

TEST_F(FramAddressEncodingTest, AddressEncoding_0x0100) {
    const uint8_t data[] = {0x00u};
    ASSERT_TRUE(driver.write(0x0100u, data, 1u));
    ASSERT_GE(spi.transmitted.size(), 4u);
    EXPECT_EQ(spi.transmitted[2], 0x01u); // addr_hi
    EXPECT_EQ(spi.transmitted[3], 0x00u); // addr_lo
}

TEST_F(FramAddressEncodingTest, AddressEncoding_0x1FFF) {
    const uint8_t data[] = {0x00u};
    ASSERT_TRUE(driver.write(0x1FFFu, data, 1u));
    ASSERT_GE(spi.transmitted.size(), 4u);
    EXPECT_EQ(spi.transmitted[2], 0x1Fu); // addr_hi
    EXPECT_EQ(spi.transmitted[3], 0xFFu); // addr_lo
}

// ---------------------------------------------------------------------------
// RangeValidation
// ---------------------------------------------------------------------------
TEST_F(FramStorageDriverTest, RangeValidation_ReadPastEnd) {
    uint8_t buf[4] = {};
    EXPECT_FALSE(driver.read(static_cast<uint32_t>(kSmallCap - 2u), buf, 4u));
    EXPECT_EQ(spi.csAssertCount, 0u); // nothing sent
}

TEST_F(FramStorageDriverTest, RangeValidation_WritePastEnd) {
    const uint8_t data[4] = {};
    EXPECT_FALSE(driver.write(static_cast<uint32_t>(kSmallCap - 2u), data, 4u));
    EXPECT_EQ(spi.csAssertCount, 0u);
}

TEST_F(FramStorageDriverTest, RangeValidation_ZeroLengthRead) {
    uint8_t buf[1] = {};
    EXPECT_FALSE(driver.read(0u, buf, 0u));
    EXPECT_EQ(spi.csAssertCount, 0u);
}

TEST_F(FramStorageDriverTest, RangeValidation_ZeroLengthWrite) {
    const uint8_t data[1] = {};
    EXPECT_FALSE(driver.write(0u, data, 0u));
    EXPECT_EQ(spi.csAssertCount, 0u);
}

// ---------------------------------------------------------------------------
// TransmitFailure
// ---------------------------------------------------------------------------
TEST_F(FramStorageDriverTest, TransmitFailure_WrenFails) {
    spi.failOnTransmit = true;

    const uint8_t data[] = {0xFFu};
    EXPECT_FALSE(driver.write(0x0000u, data, 1u));

    // CS was asserted and deasserted for the failed WREN attempt.
    EXPECT_EQ(spi.csAssertCount,   1u);
    EXPECT_EQ(spi.csDeassertCount, 1u);
    // No bytes were transmitted.
    EXPECT_TRUE(spi.transmitted.empty());
}

// ---------------------------------------------------------------------------
// ReceiveFailure
// ---------------------------------------------------------------------------
TEST_F(FramStorageDriverTest, ReceiveFailure_CsDeassertedOnError) {
    spi.failOnReceive = true;

    uint8_t buf[4] = {};
    EXPECT_FALSE(driver.read(0x0000u, buf, 4u));

    // CS must have been deasserted even though receive failed.
    EXPECT_EQ(spi.csAssertCount,   1u);
    EXPECT_EQ(spi.csDeassertCount, 1u);

    // Command bytes (opcode + addr_hi + addr_lo) were transmitted before failure.
    EXPECT_EQ(spi.transmitted.size(), 3u);
}
