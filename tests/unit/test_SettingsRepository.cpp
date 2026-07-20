#include "esf/SettingsRepository.hpp"
#include "esf/fake/FakeStorageDriver.hpp"

#include <gtest/gtest.h>

using esf::Settings;
using esf::SettingsRepository;
using esf::StorageError;
using esf::fake::FakeStorageDriver;

// A driver large enough to hold one SettingsRepository slot
static constexpr size_t kDriverSize =
    esf::StorageObject<Settings, 1u>::kTotalSize + 64u;

using Driver = FakeStorageDriver<kDriverSize>;

class SettingsRepositoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        driver.fill(0xFFu); // Simulate erased FRAM (all 0xFF)
    }

    Driver             driver;
    SettingsRepository repo{driver};
};

TEST_F(SettingsRepositoryTest, LoadFromErasedStorageReturnsInvalidMagic) {
    Settings s{};
    const auto err = repo.load(s);
    EXPECT_EQ(err, StorageError::InvalidMagic);
}

TEST_F(SettingsRepositoryTest, LoadFromErasedStorageReturnsDefaults) {
    Settings s{};
    repo.load(s);
    const Settings def = SettingsRepository::kDefault;
    EXPECT_EQ(s.displayBrightness, def.displayBrightness);
    EXPECT_EQ(s.volumeLevel,       def.volumeLevel);
    EXPECT_EQ(s.deviceId,          def.deviceId);
    EXPECT_EQ(s.featureEnabled,    def.featureEnabled);
}

TEST_F(SettingsRepositoryTest, SaveAndLoadRoundTrip) {
    Settings original{};
    original.displayBrightness = 75u;
    original.volumeLevel       = 30u;
    original.deviceId          = 0xABCDEF01u;
    original.featureEnabled    = true;

    ASSERT_EQ(repo.save(original), StorageError::Ok);

    Settings loaded{};
    ASSERT_EQ(repo.load(loaded), StorageError::Ok);

    EXPECT_EQ(loaded.displayBrightness, original.displayBrightness);
    EXPECT_EQ(loaded.volumeLevel,       original.volumeLevel);
    EXPECT_EQ(loaded.deviceId,          original.deviceId);
    EXPECT_EQ(loaded.featureEnabled,    original.featureEnabled);
}

TEST_F(SettingsRepositoryTest, ResetRestoresDefaults) {
    // Save some non-default values
    Settings modified{};
    modified.displayBrightness = 10u;
    ASSERT_EQ(repo.save(modified), StorageError::Ok);

    // Now reset
    ASSERT_EQ(repo.reset(), StorageError::Ok);

    Settings after{};
    ASSERT_EQ(repo.load(after), StorageError::Ok);

    EXPECT_EQ(after.displayBrightness,
              SettingsRepository::kDefault.displayBrightness);
}

TEST_F(SettingsRepositoryTest, CorruptedCrcReturnsError) {
    // Write valid data
    Settings s{};
    s.deviceId = 0xDEADBEEFu;
    ASSERT_EQ(repo.save(s), StorageError::Ok);

    // Corrupt a byte in the raw storage just after the header
    constexpr uint32_t corruptOffset =
        static_cast<uint32_t>(sizeof(esf::ObjectHeader)) + 0u;
    uint8_t corrupt = 0xFFu;
    driver.write(SettingsRepository::kAddress + corruptOffset, &corrupt, 1u);

    Settings loaded{};
    EXPECT_EQ(repo.load(loaded), StorageError::CrcMismatch);
}

TEST_F(SettingsRepositoryTest, DriverWriteFailureReturnedOnSave) {
    driver.setWriteFail(true);
    Settings s{};
    EXPECT_EQ(repo.save(s), StorageError::DriverError);
}

TEST_F(SettingsRepositoryTest, DriverReadFailureReturnedOnLoad) {
    // First write valid data
    ASSERT_EQ(repo.save(Settings{}), StorageError::Ok);

    driver.setReadFail(true);
    Settings s{};
    EXPECT_EQ(repo.load(s), StorageError::DriverError);
}

TEST_F(SettingsRepositoryTest, MultipleSaveOverwritesPreviousValue) {
    Settings first{};
    first.volumeLevel = 10u;
    ASSERT_EQ(repo.save(first), StorageError::Ok);

    Settings second{};
    second.volumeLevel = 99u;
    ASSERT_EQ(repo.save(second), StorageError::Ok);

    Settings loaded{};
    ASSERT_EQ(repo.load(loaded), StorageError::Ok);
    EXPECT_EQ(loaded.volumeLevel, 99u);
}
