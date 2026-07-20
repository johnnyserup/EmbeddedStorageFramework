#include "esf/examples/ExampleSettingsRepository.hpp"
#include "esf/fake/FakeStorageDriver.hpp"

#include <gtest/gtest.h>

#include <cstring>

using esf::examples::ExampleSettings;
using esf::examples::ExampleSettingsRepository;
using esf::StorageError;
using esf::fake::FakeStorageDriver;

// A driver large enough to hold one ExampleSettingsRepository slot
static constexpr size_t kDriverSize =
    ExampleSettingsRepository::kSlotSize + 64u;

using Driver = FakeStorageDriver<kDriverSize>;

namespace {

struct LegacySettingsV1 {
    uint16_t threshold = 0u;
    uint8_t  mode      = 0u;
    uint8_t  _reserved = 0u;
};
static_assert(std::is_trivially_copyable_v<LegacySettingsV1>);

struct MigratedSettingsV2 {
    uint32_t serial    = 0u;
    uint16_t threshold = 0u;
    uint8_t  mode      = 0u;
    uint8_t  _reserved = 0u;
};
static_assert(std::is_trivially_copyable_v<MigratedSettingsV2>);

class MigratingRepository final
    : public esf::RepositoryBase<MigratingRepository, MigratedSettingsV2, 2u, 0x4242u> {
public:
    static constexpr uint32_t kAddress             = 0u;
    static constexpr uint32_t kMigrationBufferSize = sizeof(LegacySettingsV1);
    static constexpr MigratedSettingsV2 kDefault{};

    explicit MigratingRepository(esf::IStorageDriver& driver) noexcept
        : RepositoryBase(driver) {}

    [[nodiscard]] static constexpr uint32_t storageAddress() noexcept {
        return kAddress;
    }

    [[nodiscard]] static constexpr MigratedSettingsV2 defaultValue() noexcept {
        return kDefault;
    }

    StorageError migrate(uint16_t       oldVersion,
                         const uint8_t* payloadBytes,
                         uint32_t       payloadSize,
                         MigratedSettingsV2& out) {
        out = defaultValue();
        if (oldVersion != 1u || payloadSize != sizeof(LegacySettingsV1)) {
            return StorageError::VersionMismatch;
        }

        LegacySettingsV1 old{};
        std::memcpy(&old, payloadBytes, sizeof(old));
        out.threshold = old.threshold;
        out.mode      = old.mode;
        out.serial    = 0xA5A5A5A5u;
        return save(out);
    }
};

} // namespace

class ExampleSettingsRepositoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        driver.fill(0xFFu); // Simulate erased FRAM (all 0xFF)
    }

    Driver                    driver;
    ExampleSettingsRepository repo{driver};
};

TEST_F(ExampleSettingsRepositoryTest, LoadFromErasedStorageReturnsInvalidMagic) {
    ExampleSettings s{};
    const auto err = repo.load(s);
    EXPECT_EQ(err, StorageError::InvalidMagic);
}

TEST_F(ExampleSettingsRepositoryTest, LoadFromErasedStorageReturnsDefaults) {
    ExampleSettings s{};
    repo.load(s);
    const ExampleSettings def = ExampleSettingsRepository::kDefault;
    EXPECT_EQ(s.displayBrightness, def.displayBrightness);
    EXPECT_EQ(s.volumeLevel,       def.volumeLevel);
    EXPECT_EQ(s.deviceId,          def.deviceId);
    EXPECT_EQ(s.featureEnabled,    def.featureEnabled);
}

TEST_F(ExampleSettingsRepositoryTest, SaveAndLoadRoundTrip) {
    ExampleSettings original{};
    original.displayBrightness = 75u;
    original.volumeLevel       = 30u;
    original.deviceId          = 0xABCDEF01u;
    original.featureEnabled    = true;

    ASSERT_EQ(repo.save(original), StorageError::Ok);

    ExampleSettings loaded{};
    ASSERT_EQ(repo.load(loaded), StorageError::Ok);

    EXPECT_EQ(loaded.displayBrightness, original.displayBrightness);
    EXPECT_EQ(loaded.volumeLevel,       original.volumeLevel);
    EXPECT_EQ(loaded.deviceId,          original.deviceId);
    EXPECT_EQ(loaded.featureEnabled,    original.featureEnabled);
}

TEST_F(ExampleSettingsRepositoryTest, ResetRestoresDefaults) {
    // Save some non-default values
    ExampleSettings modified{};
    modified.displayBrightness = 10u;
    ASSERT_EQ(repo.save(modified), StorageError::Ok);

    // Now reset
    ASSERT_EQ(repo.reset(), StorageError::Ok);

    ExampleSettings after{};
    ASSERT_EQ(repo.load(after), StorageError::Ok);

    EXPECT_EQ(after.displayBrightness,
              ExampleSettingsRepository::kDefault.displayBrightness);
}

TEST_F(ExampleSettingsRepositoryTest, CorruptedCrcReturnsError) {
    // Write valid data
    ExampleSettings s{};
    s.deviceId = 0xDEADBEEFu;
    ASSERT_EQ(repo.save(s), StorageError::Ok);

    // Corrupt a byte in the raw storage just after the header
    constexpr uint32_t corruptOffset =
        static_cast<uint32_t>(sizeof(esf::ObjectHeader)) + 0u;
    uint8_t corrupt = 0xFFu;
    driver.write(ExampleSettingsRepository::kAddress + corruptOffset, &corrupt, 1u);

    ExampleSettings loaded{};
    EXPECT_EQ(repo.load(loaded), StorageError::CrcMismatch);
}

TEST_F(ExampleSettingsRepositoryTest, WrongObjectIdReturnsInvalidMagic) {
    ExampleSettings s{};
    s.deviceId = 0x12345678u;
    ASSERT_EQ(repo.save(s), StorageError::Ok);

    uint16_t wrongObjectId = 0x9999u;
    driver.write(ExampleSettingsRepository::kAddress + sizeof(uint16_t),
                 reinterpret_cast<const uint8_t*>(&wrongObjectId),
                 sizeof(wrongObjectId));

    ExampleSettings loaded{};
    EXPECT_EQ(repo.load(loaded), StorageError::InvalidMagic);
    EXPECT_EQ(loaded.displayBrightness, ExampleSettingsRepository::kDefault.displayBrightness);
}

TEST_F(ExampleSettingsRepositoryTest, DriverWriteFailureReturnedOnSave) {
    driver.setWriteFail(true);
    ExampleSettings s{};
    EXPECT_EQ(repo.save(s), StorageError::DriverError);
}

TEST_F(ExampleSettingsRepositoryTest, DriverReadFailureReturnedOnLoad) {
    // First write valid data
    ASSERT_EQ(repo.save(ExampleSettings{}), StorageError::Ok);

    driver.setReadFail(true);
    ExampleSettings s{};
    EXPECT_EQ(repo.load(s), StorageError::DriverError);
}

TEST_F(ExampleSettingsRepositoryTest, MultipleSaveOverwritesPreviousValue) {
    ExampleSettings first{};
    first.volumeLevel = 10u;
    ASSERT_EQ(repo.save(first), StorageError::Ok);

    ExampleSettings second{};
    second.volumeLevel = 99u;
    ASSERT_EQ(repo.save(second), StorageError::Ok);

    ExampleSettings loaded{};
    ASSERT_EQ(repo.load(loaded), StorageError::Ok);
    EXPECT_EQ(loaded.volumeLevel, 99u);
}

TEST(RepositoryMigrationTest, VersionMismatchUsesRawPayloadMigrationHook) {
    FakeStorageDriver<128u> driver;
    driver.fill(0xFFu);

    const LegacySettingsV1 legacy{.threshold = 321u, .mode = 7u, ._reserved = 0u};
    const esf::ObjectHeader header{
        .magic    = esf::ObjectHeader::kMagic,
        .objectId = 0x4242u,
        .version  = 1u,
        .reserved = 0u,
        .dataSize = static_cast<uint32_t>(sizeof(LegacySettingsV1)),
        .crc      = esf::Crc32::compute(legacy),
    };

    ASSERT_TRUE(driver.write(0u,
                             reinterpret_cast<const uint8_t*>(&header),
                             sizeof(header)));
    ASSERT_TRUE(driver.write(sizeof(header),
                             reinterpret_cast<const uint8_t*>(&legacy),
                             sizeof(legacy)));

    MigratingRepository repo{driver};
    MigratedSettingsV2  out{};
    ASSERT_EQ(repo.load(out), StorageError::Ok);
    EXPECT_EQ(out.threshold, legacy.threshold);
    EXPECT_EQ(out.mode, legacy.mode);
    EXPECT_EQ(out.serial, 0xA5A5A5A5u);
}
