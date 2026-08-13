#include "esf/DualSlotRepositoryBase.hpp"
#include "esf/fake/FakeStorageDriver.hpp"
#include "esf/Crc32.hpp"

#include <gtest/gtest.h>

#include <cstring>

using esf::StorageError;
using esf::ObjectHeader;
using esf::Crc32;
using esf::fake::FakeStorageDriver;

// ---------------------------------------------------------------------------
// Test data types
// ---------------------------------------------------------------------------

struct Settings {
    uint32_t deviceId  = 0u;
    uint16_t threshold = 0u;
    uint8_t  mode      = 0u;
    uint8_t  _reserved = 0u;
};
static_assert(std::is_trivially_copyable_v<Settings>);
static_assert(std::is_standard_layout_v<Settings>);

// ---------------------------------------------------------------------------
// Concrete dual-slot repository used in most tests
// ---------------------------------------------------------------------------

inline constexpr uint16_t kTestObjectId = 0x3000u;
inline constexpr uint16_t kTestVersion  = 1u;

class SettingsDualRepo final
    : public esf::DualSlotRepositoryBase<SettingsDualRepo, Settings, kTestVersion, kTestObjectId>
{
public:
    static constexpr uint32_t kAddress  = 0u;
    static constexpr Settings kDefault{};

    explicit SettingsDualRepo(esf::IStorageDriver& driver) noexcept
        : DualSlotRepositoryBase(driver) {}

    [[nodiscard]] static constexpr uint32_t storageAddress() noexcept { return kAddress; }
    [[nodiscard]] static constexpr Settings defaultValue()   noexcept { return kDefault; }
};

// Driver must hold two slots plus a little guard space.
static constexpr size_t kDriverSize =
    SettingsDualRepo::kTotalStorageSize + 64u;

using Driver = FakeStorageDriver<kDriverSize>;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * Manually write a raw slot header (with explicit generation in reserved)
 * plus payload to the driver at @p slotAddr, without going through the
 * repository so tests can craft arbitrary slot states.
 */
static void writeRawSlot(Driver& drv,
                          uint32_t slotAddr,
                          const Settings& data,
                          uint16_t objectId,
                          uint16_t version,
                          uint16_t generation)
{
    ObjectHeader hdr{};
    hdr.magic    = ObjectHeader::kMagic;
    hdr.objectId = objectId;
    hdr.version  = version;
    hdr.reserved = generation;
    hdr.dataSize = static_cast<uint32_t>(sizeof(Settings));
    hdr.crc      = Crc32::compute(data);

    drv.write(slotAddr,
              reinterpret_cast<const uint8_t*>(&hdr),  // NOLINT
              sizeof(hdr));
    drv.write(slotAddr + static_cast<uint32_t>(sizeof(hdr)),
              reinterpret_cast<const uint8_t*>(&data),  // NOLINT
              sizeof(data));
}

static constexpr uint32_t kSlotA = SettingsDualRepo::kAddress;
static constexpr uint32_t kSlotB = SettingsDualRepo::kAddress + SettingsDualRepo::kSlotSize;

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class DualSlotRepositoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        driver.fill(0xFFu); // simulate erased FRAM
    }

    Driver            driver;
    SettingsDualRepo  repo{driver};
};

// ---------------------------------------------------------------------------
// Load from erased storage
// ---------------------------------------------------------------------------

TEST_F(DualSlotRepositoryTest, LoadFromErasedStorageReturnsInvalidMagic) {
    Settings s{};
    EXPECT_EQ(repo.load(s), StorageError::InvalidMagic);
}

TEST_F(DualSlotRepositoryTest, LoadFromErasedStorageReturnsDefaults) {
    Settings s{.deviceId = 0xDEADBEEFu};
    repo.load(s);
    EXPECT_EQ(s.deviceId,  SettingsDualRepo::kDefault.deviceId);
    EXPECT_EQ(s.threshold, SettingsDualRepo::kDefault.threshold);
    EXPECT_EQ(s.mode,      SettingsDualRepo::kDefault.mode);
}

// ---------------------------------------------------------------------------
// Basic save / load round-trip
// ---------------------------------------------------------------------------

TEST_F(DualSlotRepositoryTest, SaveAndLoadRoundTrip) {
    Settings original{.deviceId = 0x12345678u, .threshold = 500u, .mode = 3u};
    ASSERT_EQ(repo.save(original), StorageError::Ok);

    Settings loaded{};
    ASSERT_EQ(repo.load(loaded), StorageError::Ok);
    EXPECT_EQ(loaded.deviceId,  original.deviceId);
    EXPECT_EQ(loaded.threshold, original.threshold);
    EXPECT_EQ(loaded.mode,      original.mode);
}

TEST_F(DualSlotRepositoryTest, MultipleSavesReturnMostRecent) {
    Settings first{.deviceId = 1u};
    ASSERT_EQ(repo.save(first), StorageError::Ok);

    Settings second{.deviceId = 2u};
    ASSERT_EQ(repo.save(second), StorageError::Ok);

    Settings third{.deviceId = 3u};
    ASSERT_EQ(repo.save(third), StorageError::Ok);

    Settings loaded{};
    ASSERT_EQ(repo.load(loaded), StorageError::Ok);
    EXPECT_EQ(loaded.deviceId, 3u);
}

// ---------------------------------------------------------------------------
// Reset
// ---------------------------------------------------------------------------

TEST_F(DualSlotRepositoryTest, ResetRestoresDefaults) {
    Settings modified{.deviceId = 42u};
    ASSERT_EQ(repo.save(modified), StorageError::Ok);
    ASSERT_EQ(repo.reset(), StorageError::Ok);

    Settings after{};
    ASSERT_EQ(repo.load(after), StorageError::Ok);
    EXPECT_EQ(after.deviceId, SettingsDualRepo::kDefault.deviceId);
}

// ---------------------------------------------------------------------------
// Two-slot ping-pong (writes alternate between slots)
// ---------------------------------------------------------------------------

TEST_F(DualSlotRepositoryTest, FirstSaveWritesToSlotA) {
    Settings s{.deviceId = 0xAAAAu};
    ASSERT_EQ(repo.save(s), StorageError::Ok);

    // Slot A should have valid magic.
    ObjectHeader hdrA{};
    driver.read(kSlotA,
                reinterpret_cast<uint8_t*>(&hdrA),  // NOLINT
                sizeof(hdrA));
    EXPECT_EQ(hdrA.magic, ObjectHeader::kMagic);
    EXPECT_EQ(hdrA.reserved, 1u); // generation 1

    // Slot B should still be all 0xFF (erased).
    ObjectHeader hdrB{};
    driver.read(kSlotB,
                reinterpret_cast<uint8_t*>(&hdrB),  // NOLINT
                sizeof(hdrB));
    EXPECT_NE(hdrB.magic, ObjectHeader::kMagic);
}

TEST_F(DualSlotRepositoryTest, SecondSaveWritesToSlotB) {
    Settings s1{.deviceId = 1u};
    Settings s2{.deviceId = 2u};
    ASSERT_EQ(repo.save(s1), StorageError::Ok);
    ASSERT_EQ(repo.save(s2), StorageError::Ok);

    ObjectHeader hdrB{};
    driver.read(kSlotB,
                reinterpret_cast<uint8_t*>(&hdrB),  // NOLINT
                sizeof(hdrB));
    EXPECT_EQ(hdrB.magic,    ObjectHeader::kMagic);
    EXPECT_EQ(hdrB.reserved, 2u); // generation 2 (higher than slot A's 1)
}

TEST_F(DualSlotRepositoryTest, ThirdSaveWritesBackToSlotA) {
    Settings s{};
    ASSERT_EQ(repo.save(s), StorageError::Ok); // gen 1 → slot A
    ASSERT_EQ(repo.save(s), StorageError::Ok); // gen 2 → slot B
    ASSERT_EQ(repo.save(s), StorageError::Ok); // gen 3 → slot A

    ObjectHeader hdrA{};
    driver.read(kSlotA,
                reinterpret_cast<uint8_t*>(&hdrA),  // NOLINT
                sizeof(hdrA));
    EXPECT_EQ(hdrA.reserved, 3u); // generation 3

    ObjectHeader hdrB{};
    driver.read(kSlotB,
                reinterpret_cast<uint8_t*>(&hdrB),  // NOLINT
                sizeof(hdrB));
    EXPECT_EQ(hdrB.reserved, 2u); // slot B still at generation 2
}

// ---------------------------------------------------------------------------
// Power-loss resilience
// ---------------------------------------------------------------------------

TEST_F(DualSlotRepositoryTest, PowerLossDuringSecondWrite_SlotAStillReadable) {
    // First save succeeds (slot A, gen 1).
    Settings first{.deviceId = 0xAAAAu};
    ASSERT_EQ(repo.save(first), StorageError::Ok);

    // Simulate power loss mid-write to slot B: write a corrupted header
    // (valid magic but wrong CRC implies torn write).
    ObjectHeader tornHdr{};
    tornHdr.magic    = ObjectHeader::kMagic;
    tornHdr.objectId = kTestObjectId;
    tornHdr.version  = kTestVersion;
    tornHdr.reserved = 2u; // generation 2
    tornHdr.dataSize = static_cast<uint32_t>(sizeof(Settings));
    tornHdr.crc      = 0xDEADBEEFu; // wrong CRC — power loss before payload written
    driver.write(kSlotB,
                 reinterpret_cast<const uint8_t*>(&tornHdr),  // NOLINT
                 sizeof(tornHdr));

    // load() must recover slot A.
    Settings loaded{};
    ASSERT_EQ(repo.load(loaded), StorageError::Ok);
    EXPECT_EQ(loaded.deviceId, first.deviceId);
}

TEST_F(DualSlotRepositoryTest, PowerLossDuringFirstWrite_ReturnsDefaults) {
    // Slot A has a torn header, slot B is erased.
    ObjectHeader tornHdr{};
    tornHdr.magic    = ObjectHeader::kMagic;
    tornHdr.objectId = kTestObjectId;
    tornHdr.version  = kTestVersion;
    tornHdr.reserved = 1u;
    tornHdr.dataSize = static_cast<uint32_t>(sizeof(Settings));
    tornHdr.crc      = 0xBAD0BAD0u; // CRC mismatch
    driver.write(kSlotA,
                 reinterpret_cast<const uint8_t*>(&tornHdr),  // NOLINT
                 sizeof(tornHdr));

    Settings loaded{};
    EXPECT_EQ(repo.load(loaded), StorageError::InvalidMagic);
    EXPECT_EQ(loaded.deviceId, SettingsDualRepo::kDefault.deviceId);
}

TEST_F(DualSlotRepositoryTest, PowerLossDuringThirdWrite_SlotBStillReadable) {
    // gen 1 → slot A, gen 2 → slot B.
    Settings data1{.deviceId = 1u};
    Settings data2{.deviceId = 2u};
    ASSERT_EQ(repo.save(data1), StorageError::Ok);
    ASSERT_EQ(repo.save(data2), StorageError::Ok);

    // Torn write to slot A (gen 3) — power loss before payload written.
    ObjectHeader tornHdr{};
    tornHdr.magic    = ObjectHeader::kMagic;
    tornHdr.objectId = kTestObjectId;
    tornHdr.version  = kTestVersion;
    tornHdr.reserved = 3u;
    tornHdr.dataSize = static_cast<uint32_t>(sizeof(Settings));
    tornHdr.crc      = 0xCAFECAFEu; // bad CRC
    driver.write(kSlotA,
                 reinterpret_cast<const uint8_t*>(&tornHdr),  // NOLINT
                 sizeof(tornHdr));

    // Must fall back to slot B (gen 2, data2).
    Settings loaded{};
    ASSERT_EQ(repo.load(loaded), StorageError::Ok);
    EXPECT_EQ(loaded.deviceId, 2u);
}

// ---------------------------------------------------------------------------
// CRC corruption detection
// ---------------------------------------------------------------------------

TEST_F(DualSlotRepositoryTest, ActiveSlotCrcCorruptionFallsBackToOtherSlot) {
    Settings data1{.deviceId = 10u};
    Settings data2{.deviceId = 20u};
    ASSERT_EQ(repo.save(data1), StorageError::Ok); // slot A, gen 1
    ASSERT_EQ(repo.save(data2), StorageError::Ok); // slot B, gen 2

    // Corrupt active slot (B) payload.
    const uint32_t corruptAddr = kSlotB + static_cast<uint32_t>(sizeof(ObjectHeader));
    uint8_t bad = 0xFFu;
    driver.write(corruptAddr, &bad, 1u);

    // Should fall back to slot A.
    Settings loaded{};
    ASSERT_EQ(repo.load(loaded), StorageError::Ok);
    EXPECT_EQ(loaded.deviceId, 10u);
}

TEST_F(DualSlotRepositoryTest, BothSlotsCorruptedReturnsInvalidMagic) {
    // Write two valid slots then corrupt both CRCs.
    Settings data{.deviceId = 99u};
    ASSERT_EQ(repo.save(data), StorageError::Ok);
    ASSERT_EQ(repo.save(data), StorageError::Ok);

    // Corrupt payload byte in slot A.
    uint8_t bad = 0x00u;
    driver.write(kSlotA + static_cast<uint32_t>(sizeof(ObjectHeader)), &bad, 1u);
    // Corrupt payload byte in slot B.
    driver.write(kSlotB + static_cast<uint32_t>(sizeof(ObjectHeader)), &bad, 1u);

    Settings loaded{};
    EXPECT_EQ(repo.load(loaded), StorageError::InvalidMagic);
    EXPECT_EQ(loaded.deviceId, SettingsDualRepo::kDefault.deviceId);
}

// ---------------------------------------------------------------------------
// Wrong objectId
// ---------------------------------------------------------------------------

TEST_F(DualSlotRepositoryTest, WrongObjectIdInBothSlotsReturnsInvalidMagic) {
    Settings data{.deviceId = 7u};
    writeRawSlot(driver, kSlotA, data, 0x9999u, kTestVersion, 1u);
    writeRawSlot(driver, kSlotB, data, 0x9999u, kTestVersion, 2u);

    Settings loaded{};
    EXPECT_EQ(repo.load(loaded), StorageError::InvalidMagic);
}

// ---------------------------------------------------------------------------
// Driver fault injection
// ---------------------------------------------------------------------------

TEST_F(DualSlotRepositoryTest, DriverReadFailureDuringLoad_BothSlotsUnreadable) {
    // When all reads fail, both slots appear invalid → InvalidMagic + defaults.
    // (The dual-slot layer cannot distinguish a driver error from an erased slot
    //  during the header-scan phase; it reports InvalidMagic so the caller can
    //  fall back to factory defaults safely.)
    Settings data{.deviceId = 5u};
    ASSERT_EQ(repo.save(data), StorageError::Ok);

    driver.setReadFail(true);
    Settings loaded{.deviceId = 0xDEADu};
    const auto err = repo.load(loaded);
    // Either DriverError or InvalidMagic is acceptable; the important guarantee
    // is that loaded is set to defaults when the driver is completely unreadable.
    EXPECT_TRUE(err == StorageError::DriverError || err == StorageError::InvalidMagic);
    EXPECT_EQ(loaded.deviceId, SettingsDualRepo::kDefault.deviceId);
}

TEST_F(DualSlotRepositoryTest, DriverWriteFailureDuringSaveReturnsDriverError) {
    driver.setWriteFail(true);
    Settings data{.deviceId = 5u};
    EXPECT_EQ(repo.save(data), StorageError::DriverError);
}

// ---------------------------------------------------------------------------
// Generation wrapping (uint16_t overflow)
// ---------------------------------------------------------------------------

TEST_F(DualSlotRepositoryTest, GenerationWrapsAroundCorrectly) {
    // Plant slot A with generation = 0xFFFE, slot B with generation = 0xFFFF.
    Settings dataA{.deviceId = 0xAAAAu};
    Settings dataB{.deviceId = 0xBBBBu};
    writeRawSlot(driver, kSlotA, dataA, kTestObjectId, kTestVersion, 0xFFFEu);
    writeRawSlot(driver, kSlotB, dataB, kTestObjectId, kTestVersion, 0xFFFFu);

    // B (0xFFFF) is newer than A (0xFFFE) — load should return B.
    Settings loaded{};
    ASSERT_EQ(repo.load(loaded), StorageError::Ok);
    EXPECT_EQ(loaded.deviceId, 0xBBBBu);

    // Next save should wrap generation to 0x0000 and write to slot A.
    Settings newData{.deviceId = 0xCCCCu};
    ASSERT_EQ(repo.save(newData), StorageError::Ok);

    // 0x0000 wraps after 0xFFFF and is considered newer by wrapping comparison.
    Settings afterWrap{};
    ASSERT_EQ(repo.load(afterWrap), StorageError::Ok);
    EXPECT_EQ(afterWrap.deviceId, 0xCCCCu);
}

// ---------------------------------------------------------------------------
// Migration (version mismatch)
// ---------------------------------------------------------------------------

namespace {

struct LegacyV1 {
    uint16_t threshold = 0u;
    uint8_t  mode      = 0u;
    uint8_t  _reserved = 0u;
};
static_assert(std::is_trivially_copyable_v<LegacyV1>);

struct CurrentV2 {
    uint32_t deviceId  = 0u;
    uint16_t threshold = 0u;
    uint8_t  mode      = 0u;
    uint8_t  _reserved = 0u;
};
static_assert(std::is_trivially_copyable_v<CurrentV2>);

class MigratingDualRepo final
    : public esf::DualSlotRepositoryBase<MigratingDualRepo, CurrentV2, 2u, 0x3001u>
{
public:
    static constexpr uint32_t kAddress             = 0u;
    static constexpr uint32_t kMigrationBufferSize = sizeof(LegacyV1);
    static constexpr CurrentV2 kDefault{};

    explicit MigratingDualRepo(esf::IStorageDriver& driver) noexcept
        : DualSlotRepositoryBase(driver) {}

    [[nodiscard]] static constexpr uint32_t storageAddress() noexcept { return kAddress; }
    [[nodiscard]] static constexpr CurrentV2 defaultValue()  noexcept { return kDefault; }

    StorageError migrate(uint16_t       oldVersion,
                         const uint8_t* payloadBytes,
                         uint32_t       payloadSize,
                         CurrentV2&     out) {
        out = defaultValue();
        if (oldVersion != 1u || payloadSize != sizeof(LegacyV1)) {
            return StorageError::VersionMismatch;
        }
        LegacyV1 old{};
        std::memcpy(&old, payloadBytes, sizeof(old));
        out.threshold = old.threshold;
        out.mode      = old.mode;
        out.deviceId  = 0xDEADBEEFu;
        return save(out);
    }
};

} // namespace

TEST(DualSlotMigrationTest, MigratesFromV1ToV2) {
    constexpr size_t kMigDriverSize =
        MigratingDualRepo::kTotalStorageSize + 64u;
    FakeStorageDriver<kMigDriverSize> driver;
    driver.fill(0xFFu);

    // Plant a V1 slot in slot A.
    const LegacyV1 legacy{.threshold = 123u, .mode = 5u};
    ObjectHeader hdr{};
    hdr.magic    = ObjectHeader::kMagic;
    hdr.objectId = 0x3001u;
    hdr.version  = 1u;
    hdr.reserved = 1u; // generation 1
    hdr.dataSize = static_cast<uint32_t>(sizeof(LegacyV1));
    hdr.crc      = Crc32::compute(legacy);

    driver.write(0u,
                 reinterpret_cast<const uint8_t*>(&hdr),  // NOLINT
                 sizeof(hdr));
    driver.write(static_cast<uint32_t>(sizeof(hdr)),
                 reinterpret_cast<const uint8_t*>(&legacy),  // NOLINT
                 sizeof(legacy));

    MigratingDualRepo repo{driver};
    CurrentV2 out{};
    ASSERT_EQ(repo.load(out), StorageError::Ok);
    EXPECT_EQ(out.threshold, legacy.threshold);
    EXPECT_EQ(out.mode,      legacy.mode);
    EXPECT_EQ(out.deviceId,  0xDEADBEEFu);
}
