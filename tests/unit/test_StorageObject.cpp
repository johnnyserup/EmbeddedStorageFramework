#include "esf/StorageObject.hpp"

#include <gtest/gtest.h>
#include <cstdint>

using namespace esf;

// ---- Test payload type ----
struct SampleData {
    uint32_t counter = 0;
    uint8_t  flags   = 0;
    uint8_t  _pad[3] = {};
};
static_assert(std::is_trivially_copyable_v<SampleData>);

// ---- Tests ----

TEST(StorageObject, MakeProducesValidObject) {
    const SampleData d{42u, 0xABu, {}};
    const auto obj = StorageObject<SampleData, 1u, 0x2222u>::make(d);

    EXPECT_TRUE(obj.isValid());
    EXPECT_EQ(obj.header.objectId, 0x2222u);
    EXPECT_EQ(obj.data.counter, 42u);
    EXPECT_EQ(obj.data.flags, 0xABu);
}

TEST(StorageObject, DefaultConstructedIsInvalid) {
    StorageObject<SampleData> obj{};
    // Default header has magic == 0, so the object must be invalid.
    EXPECT_FALSE(obj.hasValidHeader());
    EXPECT_FALSE(obj.isValid());
}

TEST(StorageObject, CorruptedDataFailsCrcCheck) {
    const SampleData d{1u, 2u, {}};
    auto obj = StorageObject<SampleData>::make(d);
    ASSERT_TRUE(obj.isValid());

    // Flip a byte in the data payload
    obj.data.counter = 0xDEADBEEFu;

    EXPECT_TRUE(obj.hasValidHeader());  // Header is still intact
    EXPECT_FALSE(obj.hasValidCrc());    // CRC must now fail
    EXPECT_FALSE(obj.isValid());
}

TEST(StorageObject, VersionIsCorrect) {
    const auto obj = StorageObject<SampleData, 3u, 0x3333u>::make(SampleData{});
    EXPECT_EQ(obj.version(), 3u);
}

TEST(StorageObject, TotalSizeMatchesExpected) {
    // sizeof(ObjectHeader)==16, sizeof(SampleData)==8
    EXPECT_EQ((StorageObject<SampleData>::kTotalSize),
              sizeof(ObjectHeader) + sizeof(SampleData));
}

TEST(StorageObject, CorruptedHeaderMagicFailsValidation) {
    auto obj = StorageObject<SampleData>::make(SampleData{1u, 0u, {}});
    ASSERT_TRUE(obj.isValid());

    obj.header.magic ^= 0xFFFFu; // corrupt magic

    EXPECT_FALSE(obj.hasValidHeader());
    EXPECT_FALSE(obj.isValid());
}

TEST(StorageObject, CrcFieldIsUint32) {
    // Verify the CRC field in ObjectHeader is exactly 32 bits wide.
    // This guards against any accidental downgrade from CRC-32 to CRC-16.
    static_assert(sizeof(ObjectHeader{}.crc) == sizeof(uint32_t),
                  "ObjectHeader::crc must be uint32_t (CRC-32)");
    SUCCEED();
}

TEST(StorageObject, CrcValueMatchesCrc32) {
    // Verify that the CRC stored by StorageObject::make() is a valid CRC-32
    // value (i.e. it matches an independent Crc32::compute call on the data).
    using esf::Crc32;
    const SampleData d{0x12345678u, 0xABu, {}};
    const auto obj = StorageObject<SampleData>::make(d);
    EXPECT_EQ(obj.header.crc, Crc32::compute(d));
}

TEST(StorageObject, DataSizeMismatchFailsHeaderValidation) {
    auto obj = StorageObject<SampleData>::make(SampleData{});
    ASSERT_TRUE(obj.isValid());

    obj.header.dataSize = sizeof(SampleData) + 1u; // inject wrong size

    EXPECT_FALSE(obj.hasValidHeader());
    EXPECT_FALSE(obj.isValid());
}
