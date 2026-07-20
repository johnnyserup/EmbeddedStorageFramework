#include "esf/fake/FakeStorageDriver.hpp"

#include <gtest/gtest.h>
#include <cstdint>
#include <array>

using esf::fake::FakeStorageDriver;

// Use a small capacity so the test stays fast.
static constexpr size_t kCap = 256;
using Driver = FakeStorageDriver<kCap>;

class FakeStorageDriverTest : public ::testing::Test {
protected:
    void SetUp() override {
        driver.fill(0x00u);
    }
    Driver driver;
};

TEST_F(FakeStorageDriverTest, CapacityMatchesTemplate) {
    EXPECT_EQ(driver.capacity(), kCap);
}

TEST_F(FakeStorageDriverTest, WriteAndReadBack) {
    const std::array<uint8_t, 4> written = {0x11, 0x22, 0x33, 0x44};
    ASSERT_TRUE(driver.write(10u, written.data(), written.size()));

    std::array<uint8_t, 4> read{};
    ASSERT_TRUE(driver.read(10u, read.data(), read.size()));

    EXPECT_EQ(written, read);
}

TEST_F(FakeStorageDriverTest, FillInitialisesBuffer) {
    driver.fill(0xAAu);
    const auto& raw = driver.raw();
    for (size_t i = 0; i < kCap; ++i) {
        EXPECT_EQ(raw[i], 0xAAu) << "mismatch at index " << i;
    }
}

TEST_F(FakeStorageDriverTest, OutOfBoundsReadReturnsFalse) {
    uint8_t buf[4];
    EXPECT_FALSE(driver.read(kCap - 2u, buf, 4u)); // 2 bytes past end
}

TEST_F(FakeStorageDriverTest, OutOfBoundsWriteReturnsFalse) {
    const uint8_t data[4] = {};
    EXPECT_FALSE(driver.write(kCap - 2u, data, 4u));
}

TEST_F(FakeStorageDriverTest, ZeroLengthReadReturnsFalse) {
    uint8_t dummy;
    EXPECT_FALSE(driver.read(0u, &dummy, 0u));
}

TEST_F(FakeStorageDriverTest, FaultInjectionRead) {
    driver.setReadFail(true);

    uint8_t buf[4] = {};
    EXPECT_FALSE(driver.read(0u, buf, 4u));
    EXPECT_EQ(driver.readCount(), 1u);
}

TEST_F(FakeStorageDriverTest, FaultInjectionWrite) {
    driver.setWriteFail(true);

    const uint8_t data[4] = {1, 2, 3, 4};
    EXPECT_FALSE(driver.write(0u, data, 4u));
    EXPECT_EQ(driver.writeCount(), 1u);
}

TEST_F(FakeStorageDriverTest, CallCountTracking) {
    uint8_t buf[4] = {};
    const uint8_t data[4] = {1, 2, 3, 4};

    EXPECT_EQ(driver.readCount(), 0u);
    EXPECT_EQ(driver.writeCount(), 0u);

    driver.read(0u, buf, 4u);
    driver.read(0u, buf, 4u);
    driver.write(0u, data, 4u);

    EXPECT_EQ(driver.readCount(), 2u);
    EXPECT_EQ(driver.writeCount(), 1u);
}

TEST_F(FakeStorageDriverTest, FillResetsCallCounts) {
    uint8_t buf[1] = {};
    driver.read(0u, buf, 1u);
    driver.write(0u, buf, 1u);

    driver.fill();

    EXPECT_EQ(driver.readCount(), 0u);
    EXPECT_EQ(driver.writeCount(), 0u);
}

TEST_F(FakeStorageDriverTest, WritePersistsAcrossMultipleReads) {
    const uint8_t pattern[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    ASSERT_TRUE(driver.write(0u, pattern, 8u));

    for (int i = 0; i < 3; ++i) {
        uint8_t out[8] = {};
        ASSERT_TRUE(driver.read(0u, out, 8u));
        for (int j = 0; j < 8; ++j) {
            EXPECT_EQ(out[j], pattern[j]);
        }
    }
}
