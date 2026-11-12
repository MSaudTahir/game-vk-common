// Unit test of gvc's BcDecode (CPU BC1 / BC2 / BC3 decoder used when the GPU has no textureCompressionBC).
//
// Build:  g++ -std=c++17 -O2 -Iinclude tests/bc_decode_test.cpp src/BcDecode.cpp -o /tmp/gvc_bc_decode_test
// Run:    /tmp/gvc_bc_decode_test                                      -> exit code = number of failed checks
//         /tmp/gvc_bc_decode_test --decode <fmt 0|1|3|5> <w> <h> <in.bin> <out.rgba>
//             (fmt: 0 = BC1_RGB, 1 = BC1/DXT1, 3 = BC2/DXT3, 5 = BC3/DXT5; used by tools/tests/bc_decode_pil.py
//              in nfsmw, which cross-checks against Pillow)

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "gvc/BcDecode.h"

static int g_fail = 0;
#define CHECK(c, ...) do { if (!(c)) { ++g_fail; std::printf("FAIL %s:%d: ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static void put16(uint8_t* p, uint32_t v) { p[0] = v & 255; p[1] = v >> 8; }

int main(int argc, char** argv) {
    if (argc == 7 && !std::strcmp(argv[1], "--decode")) {
        const int fmt = std::atoi(argv[2]);
        const uint32_t w = std::atoi(argv[3]), h = std::atoi(argv[4]);
        const gvc::BcFormat f = fmt == 0 ? gvc::BcFormat::BC1_RGB : fmt == 1 ? gvc::BcFormat::BC1_RGBA
                               : fmt == 3 ? gvc::BcFormat::BC2 : gvc::BcFormat::BC3;
        std::vector<uint8_t> in(gvc::levelBytes(f, w, h));
        FILE* fi = std::fopen(argv[5], "rb");
        if (!fi || std::fread(in.data(), 1, in.size(), fi) != in.size()) return 2;
        std::fclose(fi);
        std::vector<uint8_t> out(static_cast<size_t>(w) * h * 4);
        gvc::decodeLevel(f, in.data(), w, h, out.data());
        FILE* fo = std::fopen(argv[6], "wb");
        std::fwrite(out.data(), 1, out.size(), fo);
        std::fclose(fo);
        return 0;
    }

    // level sizes
    CHECK(gvc::levelBytes(gvc::BcFormat::BC1_RGBA, 8, 8) == 32, "bc1 8x8");
    CHECK(gvc::levelBytes(gvc::BcFormat::BC3, 8, 8) == 64, "bc3 8x8");
    CHECK(gvc::levelBytes(gvc::BcFormat::BC1_RGBA, 1, 1) == 8, "bc1 1x1 = one block");
    CHECK(gvc::levelBytes(gvc::BcFormat::BC2, 5, 2) == 2 * 16, "bc2 5x2");
    CHECK(gvc::levelBytes(gvc::BcFormat::BC1_RGB, 5, 2) == 2 * 8, "bc1 rgb 5x2");

    // BC1 4 colour mode: c0 = pure red (0xF800), c1 = pure blue (0x001F); c0 > c1.  indices 0,1,2,3 across the
    // top row, then repeat.  Bit-replication expansion: red stays (255,0,0), blue stays (0,0,255).
    {
        uint8_t blk[8] = {};
        put16(blk, 0xF800);
        put16(blk + 2, 0x001F);
        for (int row = 0; row < 4; ++row) blk[4 + row] = 0b11100100;   // px0=0 px1=1 px2=2 px3=3
        uint8_t px[4 * 4 * 4];
        gvc::decodeLevel(gvc::BcFormat::BC1_RGBA, blk, 4, 4, px);
        CHECK(px[0] == 255 && px[1] == 0 && px[2] == 0 && px[3] == 255, "c0 red: %d %d %d %d", px[0], px[1], px[2], px[3]);
        CHECK(px[4] == 0 && px[5] == 0 && px[6] == 255 && px[7] == 255, "c1 blue");
        CHECK(px[8] == 170 && px[10] == 85 && px[11] == 255, "c2 = (2*red+blue)/3: %d %d", px[8], px[10]);
        CHECK(px[12] == 85 && px[14] == 170, "c3 = (red+2*blue)/3: %d %d", px[12], px[14]);
        // bit-replication check on a non-trivial endpoint: 565 value 0x7BEF -> r=15,g=31,b=15 -> (123,125,123)
        put16(blk, 0x7BEF);
        put16(blk + 2, 0x7BEF);
        gvc::decodeLevel(gvc::BcFormat::BC1_RGBA, blk, 4, 4, px);
        CHECK(px[0] == 123 && px[1] == 125 && px[2] == 123, "565 bit replication: %d %d %d", px[0], px[1], px[2]);
    }
    // BC1 3 colour + transparent mode (c0 <= c1): index 2 = average, index 3 = transparent black
    {
        uint8_t blk[8] = {};
        put16(blk, 0x001F);
        put16(blk + 2, 0xF800);
        blk[4] = 0b11100100;
        uint8_t px[4 * 4 * 4];
        gvc::decodeLevel(gvc::BcFormat::BC1_RGBA, blk, 4, 4, px);
        CHECK(px[8] == 127 && px[10] == 127 && px[11] == 255, "c2 average: %d %d %d", px[8], px[10], px[11]);
        CHECK(px[12] == 0 && px[13] == 0 && px[14] == 0 && px[15] == 0, "index 3 is transparent black");
        // The same block as BC1_RGB: 4 colour mode, alpha forced 255
        gvc::decodeLevel(gvc::BcFormat::BC1_RGB, blk, 4, 4, px);
        CHECK(px[8] == 85 && px[10] == 170, "bc1 rgb index 2 uses the 4 colour interpolation: %d %d", px[8], px[10]);
        CHECK(px[15] == 255, "bc1 rgb index 3 is opaque colour 3");
    }
    // BC2: explicit 4 bit alpha, always 4 colour mode even when c0 <= c1
    {
        uint8_t blk[16] = {};
        for (int i = 0; i < 8; ++i) blk[i] = static_cast<uint8_t>((2 * i) | ((2 * i + 1) << 4));   // pixel n has alpha nibble n
        put16(blk + 8, 0x001F);
        put16(blk + 10, 0xF800);   // c0 < c1: BC1 would use 3 colour mode
        blk[12] = 0b11100100;
        uint8_t px[4 * 4 * 4];
        gvc::decodeLevel(gvc::BcFormat::BC2, blk, 4, 4, px);
        CHECK(px[3] == 0 && px[7] == 17 && px[11] == 34 && px[15] == 51 && px[4 * 15 + 3] == 255, "bc2 alpha %d %d %d", px[3], px[7], px[63]);
        CHECK(px[8 + 0] == 85 && px[8 + 2] == 170, "bc2 index 2 uses the 4 colour interpolation: %d %d", px[8], px[10]);
        CHECK(px[12 + 3] == 51, "bc2 opaque colour block, alpha from the alpha block");
    }
    // BC3: alpha endpoints 255 / 0 (8 value mode), indices 0..7 in the first 8 pixels
    {
        uint8_t blk[16] = {};
        blk[0] = 255;
        blk[1] = 0;
        uint64_t bits = 0;
        for (int i = 0; i < 16; ++i) bits |= static_cast<uint64_t>(i & 7) << (3 * i);
        for (int i = 0; i < 6; ++i) blk[2 + i] = static_cast<uint8_t>(bits >> (8 * i));
        put16(blk + 8, 0xFFFF);
        put16(blk + 10, 0xFFFF);
        uint8_t px[4 * 4 * 4];
        gvc::decodeLevel(gvc::BcFormat::BC3, blk, 4, 4, px);
        const int expect[8] = {255, 0, 218, 182, 145, 109, 72, 36};   // (6*255)/7 = 218, ...
        for (int i = 0; i < 8; ++i) CHECK(px[i * 4 + 3] == expect[i], "bc3 alpha idx %d = %d, want %d", i, px[i * 4 + 3], expect[i]);
        CHECK(px[0] == 255 && px[1] == 255 && px[2] == 255, "white");
        // 6 value mode (a0 <= a1): idx 6 = 0, idx 7 = 255
        blk[0] = 0;
        blk[1] = 255;
        gvc::decodeLevel(gvc::BcFormat::BC3, blk, 4, 4, px);
        CHECK(px[6 * 4 + 3] == 0 && px[7 * 4 + 3] == 255 && px[2 * 4 + 3] == 51, "bc3 6 value mode %d %d %d", px[27], px[31], px[11]);
    }
    // non multiple of 4 sizes: 6x3 -> 2x1 blocks, only the visible pixels are written
    {
        uint8_t blk[16] = {};
        put16(blk, 0xF800);
        put16(blk + 2, 0xF800);   // all pixels index 0 = red
        std::vector<uint8_t> src(16 + 16 - 8 + 8);   // two BC1 blocks
        std::memcpy(src.data(), blk, 8);
        std::memcpy(src.data() + 8, blk, 8);
        std::vector<uint8_t> out(6 * 3 * 4, 7);
        gvc::decodeLevel(gvc::BcFormat::BC1_RGBA, src.data(), 6, 3, out.data());
        bool ok = true;
        for (int i = 0; i < 6 * 3; ++i) ok = ok && out[i * 4] == 255 && out[i * 4 + 1] == 0 && out[i * 4 + 3] == 255;
        CHECK(ok, "6x3 fully written");
    }
    // chain: 8x8 + 4x4 + 2x2 + 1x1 BC1 -> RGBA8 chain of 256 + 64 + 16 + 4 pixels*4
    {
        std::vector<uint8_t> src(32 + 8 + 8 + 8, 0);
        for (size_t o = 0; o < src.size(); o += 8) put16(&src[o], 0xFFFF);   // white block (idx 0, both endpoints white)
        auto chain = gvc::decodeChain(gvc::BcFormat::BC1_RGBA, 8, 8, 4, src.data(), src.size());
        CHECK(chain.size() == (64 + 16 + 4 + 1) * 4, "chain size %zu", chain.size());
        CHECK(chain[0] == 255 && chain[chain.size() - 1] == 255, "chain first / last pixel white");
        // truncated data: the missing levels are zero, the present ones are decoded
        auto cut = gvc::decodeChain(gvc::BcFormat::BC1_RGBA, 8, 8, 4, src.data(), 32 + 8);
        CHECK(cut[0] == 255, "truncated data: the levels that are present are decoded");
        CHECK(cut[(64 + 16) * 4 + 3] == 0, "missing level is zeros");
    }
#ifdef GVC_HAS_VULKAN
    // VkFormat API
    CHECK(gvc::isBcFormat(VK_FORMAT_BC1_RGBA_UNORM_BLOCK) && gvc::isBcFormat(VK_FORMAT_BC3_UNORM_BLOCK), "isBcFormat true cases");
    CHECK(!gvc::isBcFormat(VK_FORMAT_R8G8B8A8_UNORM) && !gvc::isBcFormat(VK_FORMAT_BC4_UNORM_BLOCK), "isBcFormat false cases");
    CHECK(gvc::bcMipSize(VK_FORMAT_BC1_RGBA_UNORM_BLOCK, 8, 8) == 32, "bcMipSize bc1");
    CHECK(gvc::bcMipSize(VK_FORMAT_BC3_UNORM_BLOCK, 8, 8) == 64, "bcMipSize bc3");
    {
        // decodeBc on BC1_RGBA must agree with decodeLevel on BC1_RGBA
        uint8_t blk[8] = {};
        put16(blk, 0xF800);
        put16(blk + 2, 0x001F);
        blk[4] = 0b11100100;
        uint8_t a[64], b[64];
        gvc::decodeBc(VK_FORMAT_BC1_RGBA_UNORM_BLOCK, blk, 4, 4, a);
        gvc::decodeLevel(gvc::BcFormat::BC1_RGBA, blk, 4, 4, b);
        CHECK(std::memcmp(a, b, 64) == 0, "decodeBc == decodeLevel");
        // BC1_RGB forces opaque
        gvc::decodeBc(VK_FORMAT_BC1_RGB_UNORM_BLOCK, blk, 4, 4, a);
        gvc::decodeLevel(gvc::BcFormat::BC1_RGB, blk, 4, 4, b);
        CHECK(std::memcmp(a, b, 64) == 0, "decodeBc BC1_RGB == decodeLevel BC1_RGB");
        CHECK(a[15] == 255, "BC1_RGB forces alpha 255");
    }
#endif
    std::printf(g_fail ? "%d checks FAILED\n" : "gvc bc_decode_test: all checks passed\n", g_fail);
    return g_fail;
}
