#include "gvc/BcDecode.h"

#include <algorithm>
#include <cstring>

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#define GVC_HAS_NEON 1
#else
#define GVC_HAS_NEON 0
#endif

namespace gvc {

size_t levelBytes(BcFormat f, uint32_t w, uint32_t h) {
    const size_t blocks = static_cast<size_t>((w + 3) / 4) * ((h + 3) / 4);
    return blocks * (f == BcFormat::BC1_RGB || f == BcFormat::BC1_RGBA ? 8 : 16);
}

namespace {

inline uint8_t expand5(uint32_t v) { return static_cast<uint8_t>((v << 3) | (v >> 2)); }
inline uint8_t expand6(uint32_t v) { return static_cast<uint8_t>((v << 2) | (v >> 4)); }

inline void colorPalette(const uint8_t* blk, bool allowPunchThrough, uint8_t pal[4][4]) {
    const uint16_t c0 = static_cast<uint16_t>(blk[0] | (blk[1] << 8));
    const uint16_t c1 = static_cast<uint16_t>(blk[2] | (blk[3] << 8));
    pal[0][0] = expand5((c0 >> 11) & 31); pal[0][1] = expand6((c0 >> 5) & 63); pal[0][2] = expand5(c0 & 31); pal[0][3] = 255;
    pal[1][0] = expand5((c1 >> 11) & 31); pal[1][1] = expand6((c1 >> 5) & 63); pal[1][2] = expand5(c1 & 31); pal[1][3] = 255;
    if (!allowPunchThrough || c0 > c1) {
        pal[2][0] = static_cast<uint8_t>((2u * pal[0][0] + pal[1][0]) / 3u);
        pal[2][1] = static_cast<uint8_t>((2u * pal[0][1] + pal[1][1]) / 3u);
        pal[2][2] = static_cast<uint8_t>((2u * pal[0][2] + pal[1][2]) / 3u); pal[2][3] = 255;
        pal[3][0] = static_cast<uint8_t>((pal[0][0] + 2u * pal[1][0]) / 3u);
        pal[3][1] = static_cast<uint8_t>((pal[0][1] + 2u * pal[1][1]) / 3u);
        pal[3][2] = static_cast<uint8_t>((pal[0][2] + 2u * pal[1][2]) / 3u); pal[3][3] = 255;
    } else {
        pal[2][0] = static_cast<uint8_t>((pal[0][0] + pal[1][0]) / 2u);
        pal[2][1] = static_cast<uint8_t>((pal[0][1] + pal[1][1]) / 2u);
        pal[2][2] = static_cast<uint8_t>((pal[0][2] + pal[1][2]) / 2u); pal[2][3] = 255;
        pal[3][0] = 0; pal[3][1] = 0; pal[3][2] = 0; pal[3][3] = 0;
    }
}

inline void bc3Alpha(const uint8_t* blk, uint8_t out[16]) {
    const uint8_t a0 = blk[0], a1 = blk[1];
    uint8_t pal[8];
    pal[0] = a0; pal[1] = a1;
    if (a0 > a1) {
        for (int i = 1; i <= 6; ++i) pal[1 + i] = static_cast<uint8_t>(((7 - i) * a0 + i * a1) / 7);
    } else {
        for (int i = 1; i <= 4; ++i) pal[1 + i] = static_cast<uint8_t>(((5 - i) * a0 + i * a1) / 5);
        pal[6] = 0; pal[7] = 255;
    }
    uint64_t bits = 0;
    for (int i = 0; i < 6; ++i) bits |= static_cast<uint64_t>(blk[2 + i]) << (8 * i);
    for (int i = 0; i < 16; ++i) out[i] = pal[(bits >> (3 * i)) & 7];
}

inline void storePixel(uint8_t* dst, uint32_t w, uint32_t x, uint32_t y, const uint8_t c[4]) {
    uint8_t* o = dst + (static_cast<size_t>(y) * w + x) * 4;
    std::memcpy(o, c, 4);
}

template<BcFormat F>
inline void decodeBlock(const uint8_t* blk, uint8_t* dst, uint32_t w, uint32_t h, uint32_t bx, uint32_t by) {
    constexpr bool bc1 = (F == BcFormat::BC1_RGB || F == BcFormat::BC1_RGBA);
    const uint8_t* colorBlk = bc1 ? blk : blk + 8;
    uint8_t pal[4][4];
    colorPalette(colorBlk, F == BcFormat::BC1_RGBA, pal);
    uint8_t alpha[16];
    if constexpr (F == BcFormat::BC2) {
        for (int i = 0; i < 16; ++i) alpha[i] = static_cast<uint8_t>(((blk[i >> 1] >> ((i & 1) * 4)) & 15) * 17);
    } else if constexpr (F == BcFormat::BC3) {
        bc3Alpha(blk, alpha);
    }
    const uint32_t idx = static_cast<uint32_t>(colorBlk[4]) | (static_cast<uint32_t>(colorBlk[5]) << 8) |
                         (static_cast<uint32_t>(colorBlk[6]) << 16) | (static_cast<uint32_t>(colorBlk[7]) << 24);
    const bool full = (bx + 4 <= w && by + 4 <= h);
    if (full) {
#if GVC_HAS_NEON
        const uint32_t pal32[4] = {
            uint32_t(pal[0][0]) | (uint32_t(pal[0][1]) << 8) | (uint32_t(pal[0][2]) << 16) | (uint32_t(pal[0][3]) << 24),
            uint32_t(pal[1][0]) | (uint32_t(pal[1][1]) << 8) | (uint32_t(pal[1][2]) << 16) | (uint32_t(pal[1][3]) << 24),
            uint32_t(pal[2][0]) | (uint32_t(pal[2][1]) << 8) | (uint32_t(pal[2][2]) << 16) | (uint32_t(pal[2][3]) << 24),
            uint32_t(pal[3][0]) | (uint32_t(pal[3][1]) << 8) | (uint32_t(pal[3][2]) << 16) | (uint32_t(pal[3][3]) << 24)
        };
        for (uint32_t py = 0; py < 4; ++py) {
            uint8_t* row = dst + (static_cast<size_t>(by + py) * w + bx) * 4;
            for (uint32_t px = 0; px < 4; ++px) {
                const uint32_t i = py * 4 + px;
                uint32_t c = pal32[(idx >> (2 * i)) & 3];
                if constexpr (F == BcFormat::BC2 || F == BcFormat::BC3) c = (c & 0x00FFFFFFu) | (uint32_t(alpha[i]) << 24);
                else if constexpr (F == BcFormat::BC1_RGB) c |= 0xFF000000u;
                std::memcpy(row + px * 4, &c, 4);
            }
        }
#else
        for (uint32_t py = 0; py < 4; ++py) {
            uint8_t* row = dst + (static_cast<size_t>(by + py) * w + bx) * 4;
            for (uint32_t px = 0; px < 4; ++px) {
                const uint32_t i = py * 4 + px;
                const uint8_t* c = pal[(idx >> (2 * i)) & 3];
                uint8_t* o = row + px * 4;
                o[0] = c[0]; o[1] = c[1]; o[2] = c[2];
                if constexpr (F == BcFormat::BC2 || F == BcFormat::BC3) o[3] = alpha[i];
                else if constexpr (F == BcFormat::BC1_RGB) o[3] = 255;
                else o[3] = c[3];
            }
        }
#endif
    } else {
        for (uint32_t py = 0; py < 4; ++py) {
            const uint32_t y = by + py; if (y >= h) break;
            for (uint32_t px = 0; px < 4; ++px) {
                const uint32_t x = bx + px; if (x >= w) break;
                const uint32_t i = py * 4 + px;
                const uint8_t* c = pal[(idx >> (2 * i)) & 3];
                uint8_t out[4] = {c[0], c[1], c[2], 255};
                if constexpr (F == BcFormat::BC2 || F == BcFormat::BC3) out[3] = alpha[i];
                else if constexpr (F == BcFormat::BC1_RGBA) out[3] = c[3];
                storePixel(dst, w, x, y, out);
            }
        }
    }
}

template<BcFormat F>
void decodeLevelT(const uint8_t* src, uint32_t w, uint32_t h, uint8_t* dst) {
    constexpr bool bc1 = (F == BcFormat::BC1_RGB || F == BcFormat::BC1_RGBA);
    constexpr size_t stride = bc1 ? 8 : 16;
    const uint32_t bw = (w + 3) / 4, bh = (h + 3) / 4;
    for (uint32_t by = 0; by < bh; ++by) {
        for (uint32_t bx = 0; bx < bw; ++bx) {
            const uint8_t* blk = src + (static_cast<size_t>(by) * bw + bx) * stride;
            decodeBlock<F>(blk, dst, w, h, bx * 4, by * 4);
        }
    }
}

} // namespace

void decodeLevel(BcFormat f, const uint8_t* src, uint32_t w, uint32_t h, uint8_t* dst) {
    switch (f) {
        case BcFormat::BC1_RGB:  decodeLevelT<BcFormat::BC1_RGB>(src, w, h, dst); break;
        case BcFormat::BC1_RGBA: decodeLevelT<BcFormat::BC1_RGBA>(src, w, h, dst); break;
        case BcFormat::BC2:      decodeLevelT<BcFormat::BC2>(src, w, h, dst); break;
        case BcFormat::BC3:      decodeLevelT<BcFormat::BC3>(src, w, h, dst); break;
    }
}

std::vector<uint8_t> decodeChain(BcFormat f, uint32_t w, uint32_t h, uint32_t levels,
                                 const uint8_t* src, size_t srcSize) {
    size_t total = 0;
    uint32_t mw = w, mh = h;
    for (uint32_t l = 0; l < levels; ++l) {
        total += static_cast<size_t>(mw) * mh * 4;
        mw = std::max(1u, mw / 2); mh = std::max(1u, mh / 2);
    }
    std::vector<uint8_t> out(total, 0);
    size_t so = 0, dO = 0;
    mw = w; mh = h;
    for (uint32_t l = 0; l < levels; ++l) {
        const size_t sb = levelBytes(f, mw, mh);
        if (so + sb <= srcSize) decodeLevel(f, src + so, mw, mh, out.data() + dO);
        so += sb;
        dO += static_cast<size_t>(mw) * mh * 4;
        mw = std::max(1u, mw / 2); mh = std::max(1u, mh / 2);
    }
    return out;
}

#ifdef GVC_HAS_VULKAN
bool isBcFormat(VkFormat f) {
    return f == VK_FORMAT_BC1_RGB_UNORM_BLOCK || f == VK_FORMAT_BC1_RGBA_UNORM_BLOCK ||
           f == VK_FORMAT_BC2_UNORM_BLOCK || f == VK_FORMAT_BC3_UNORM_BLOCK;
}

size_t bcMipSize(VkFormat f, uint32_t w, uint32_t h) {
    BcFormat g = BcFormat::BC3;
    if (f == VK_FORMAT_BC1_RGB_UNORM_BLOCK) g = BcFormat::BC1_RGB;
    else if (f == VK_FORMAT_BC1_RGBA_UNORM_BLOCK) g = BcFormat::BC1_RGBA;
    else if (f == VK_FORMAT_BC2_UNORM_BLOCK) g = BcFormat::BC2;
    return levelBytes(g, w, h);
}

void decodeBc(VkFormat f, const uint8_t* src, uint32_t w, uint32_t h, uint8_t* dst) {
    BcFormat g = BcFormat::BC3;
    if (f == VK_FORMAT_BC1_RGB_UNORM_BLOCK) g = BcFormat::BC1_RGB;
    else if (f == VK_FORMAT_BC1_RGBA_UNORM_BLOCK) g = BcFormat::BC1_RGBA;
    else if (f == VK_FORMAT_BC2_UNORM_BLOCK) g = BcFormat::BC2;
    decodeLevel(g, src, w, h, dst);
}
#endif

} // namespace gvc
