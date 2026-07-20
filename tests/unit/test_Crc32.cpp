#include "esf/Crc32.hpp"

#include <gtest/gtest.h>
#include <cstdint>
#include <array>

using esf::Crc32;

// ---- Known-good reference values (verified against external CRC-32 tools) ----

TEST(Crc32, EmptyBufferReturnsZero) {
    // CRC of zero bytes with the standard CRC-32 algorithm and full
    // finalisation XOR should equal 0x00000000 for empty input.
    // NOTE: compute(nullptr, 0) is valid because the while-loop body is
    // never entered.
    const uint32_t result = Crc32::compute(nullptr, 0u);
    EXPECT_EQ(result, 0x00000000u);
}

TEST(Crc32, SingleZeroByte) {
    const uint8_t data = 0x00u;
    // ISO 3309 / zlib CRC-32 of a single 0x00 byte = 0xD202EF8D
    EXPECT_EQ(Crc32::compute(&data, 1u), 0xD202EF8Du);
}

TEST(Crc32, SingleByteFF) {
    const uint8_t data = 0xFFu;
    EXPECT_EQ(Crc32::compute(&data, 1u), 0xFF000000u);
}

TEST(Crc32, KnownStringABC) {
    // CRC-32 of ASCII "ABC" = 0xA3830348
    const std::array<uint8_t, 3> data = {0x41u, 0x42u, 0x43u};
    EXPECT_EQ(Crc32::compute(data.data(), static_cast<uint32_t>(data.size())),
              0xA3830348u);
}

TEST(Crc32, KnownString123456789) {
    // CRC-32 of "123456789" (standard check value) = 0xCBF43926
    const char str[] = "123456789";
    const uint32_t result = Crc32::compute(
        reinterpret_cast<const uint8_t*>(str),
        static_cast<uint32_t>(sizeof(str) - 1u) // exclude NUL
    );
    EXPECT_EQ(result, 0xCBF43926u);
}

TEST(Crc32, ChainedCallsMatchSingleCall) {
    // CRC computed in two parts must equal a single full-buffer computation.
    const std::array<uint8_t, 6> full = {1, 2, 3, 4, 5, 6};

    const uint32_t single = Crc32::compute(full.data(), 6u);

    // Compute first half, then continue from that intermediate value
    // (re-invert to feed back into compute with custom init).
    // Actually, the standard chaining trick for this finalised CRC is:
    //   pass (result ^ 0xFFFFFFFF) as 'init' so the XOR at the end cancels.
    const uint32_t part1 = Crc32::compute(full.data(), 3u);
    const uint32_t part2 = Crc32::compute(full.data() + 3u, 3u,
                                           part1 ^ 0xFFFFFFFFu);
    EXPECT_EQ(part2, single);
}

TEST(Crc32, TemplateOverloadMatchesRawOverload) {
    struct Pod {
        uint32_t x = 0xDEADBEEFu;
        uint16_t y = 0xCAFEu;
    };
    const Pod pod{};

    const uint32_t via_template = Crc32::compute(pod);
    const uint32_t via_raw      = Crc32::compute(
        reinterpret_cast<const uint8_t*>(&pod), sizeof(pod));

    EXPECT_EQ(via_template, via_raw);
}
