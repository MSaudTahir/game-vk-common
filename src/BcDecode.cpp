#include "gvc/BcDecode.h"

#include <algorithm>

namespace gvc {

size_t levelBytes(BcFormat f, uint32_t w, uint32_t h) {
    const size_t blocks = static_cast<size_t>((w + 3) / 4) * ((h + 3) / 4);
    return blocks * (f == BcFormat::BC1_RGB || f == BcFormat::BC1_RGBA ? 8 : 16);
}

namespace {

// The four colours of a colour block (RGBA8 each).  565 -> 8 bit expansion is bit replication
// ((r << 3) | (r >> 2)), the hardware-accurate form.  `allowPunchThrough` is only true for BC1_RGBA:
// BC2 / BC3 colour halves and BC1_RGB always use the opaque 4 colour mode.
void colorPalette(const uint8_t* blk, bool allowPunchThrough, uint8_t pal[4][4]) {
    const uint16_t c0 = static_cast<uint16_t>(blk[0] | (blk[1] << 8));
    const uint16_t c1 = static_cast<uint16_t>(blk[2] | (blk[3] << 8));
    auto expand = [](uint16_t c, uint8_t* o) {
        o[0] = static_cast<uint8_t>((((c >> 11) & 31) << 3) | (((c >> 11) & 31) >> 2));
        o[1] = static_cast<uint8_t>((((c >> 5) & 63) << 2) | (((c >> 5) & 63) >> 4));
        o[2] = static_cast<uint8_t>(((c & 31) << 3) | ((c & 31) >> 2));
        o[3] = 255;
    };
    expand(c0, pal[0]);
    expand(c1, pal[1]);
    if (!allowPunchThrough || c0 > c1) {
        for (int k = 0; k < 3; ++k) {
            pal[2][k] = static_cast<uint8_t>((2 * pal[0][k] + pal[1][k]) / 3);
            pal[3][k] = static_cast<uint8_t>((pal[0][k] + 2 * pal[1][k]) / 3);
        }
        pal[2][3] = pal[3][3] = 255;
    } else { // 3 colours + transparent black
        for (int k = 0; k < 3; ++k) pal[2][k] = static_cast<uint8_t>((pal[0][k] + pal[1][k]) / 2);
        pal[2][3] = 255;
        pal[3][0] = pal[3][1] = pal[3][2] = 0;
        pal[3][3] = 0;
    }
}

// BC3 alpha block (8 bytes) -> 16 alpha values, row major.
void bc3Alpha(const uint8_t* blk, uint8_t out[16]) {
    const uint8_t a0 = blk[0], a1 = blk[1];
    uint8_t pal[8];
    pal[0] = a0;
    pal[1] = a1;
    if (a0 > a1) {
        for (int i = 1; i <= 6; ++i) pal[1 + i] = static_cast<uint8_t>(((7 - i) * a0 + i * a1) / 7);
    } else {
        for (int i = 1; i <= 4; ++i) pal[1 + i] = static_cast<uint8_t>(((5 - i) * a0 + i * a1) / 5);
        pal[6] = 0;
        pal[7] = 255;
    }
    uint64_t bits = 0;
    for (int i = 0; i < 6; ++i) bits |= static_cast<uint64_t>(blk[2 + i]) << (8 * i);
    for (int i = 0; i < 16; ++i) out[i] = pal[(bits >> (3 * i)) & 7];
}

} // namespace

void decodeLevel(BcFormat f, const uint8_t* src, uint32_t w, uint32_t h, uint8_t* dst) {
    const bool bc1 = f == BcFormat::BC1_RGB || f == BcFormat::BC1_RGBA;
    const size_t stride = bc1 ? 8 : 16;
    const uint32_t bw = (w + 3) / 4, bh = (h + 3) / 4;
    for (uint32_t by = 0; by < bh; ++by) {
        for (uint32_t bx = 0; bx < bw; ++bx) {
            const uint8_t* blk = src + (static_cast<size_t>(by) * bw + bx) * stride;
            const uint8_t* colorBlk = bc1 ? blk : blk + 8;
            uint8_t pal[4][4];
            colorPalette(colorBlk, f == BcFormat::BC1_RGBA, pal);
            uint8_t alpha[16];
            if (f == BcFormat::BC2) {
                for (int i = 0; i < 16; ++i) {
                    const uint8_t nib = (blk[i >> 1] >> ((i & 1) * 4)) & 15;
                    alpha[i] = static_cast<uint8_t>(nib * 17);
                }
            } else if (f == BcFormat::BC3) {
                bc3Alpha(blk, alpha);
            }
            const uint32_t idx = static_cast<uint32_t>(colorBlk[4]) | (static_cast<uint32_t>(colorBlk[5]) << 8) |
                                 (static_cast<uint32_t>(colorBlk[6]) << 16) | (static_cast<uint32_t>(colorBlk[7]) << 24);
            for (uint32_t py = 0; py < 4; ++py) {
                const uint32_t y = by * 4 + py;
                if (y >= h) break;
                for (uint32_t px = 0; px < 4; ++px) {
                    const uint32_t x = bx * 4 + px;
                    if (x >= w) break;
                    const uint32_t i = py * 4 + px;
                    const uint8_t* c = pal[(idx >> (2 * i)) & 3];
                    uint8_t* o = dst + (static_cast<size_t>(y) * w + x) * 4;
                    o[0] = c[0];
                    o[1] = c[1];
                    o[2] = c[2];
                    o[3] = f == BcFormat::BC2 || f == BcFormat::BC3 ? alpha[i]
                           : f == BcFormat::BC1_RGB ? 255 : c[3];
                }
            }
        }
    }
}

std::vector<uint8_t> decodeChain(BcFormat f, uint32_t w, uint32_t h, uint32_t levels,
                                 const uint8_t* src, size_t srcSize) {
    size_t total = 0;
    {
        uint32_t mw = w, mh = h;
        for (uint32_t l = 0; l < levels; ++l) {
            total += static_cast<size_t>(mw) * mh * 4;
            mw = std::max(1u, mw / 2);
            mh = std::max(1u, mh / 2);
        }
    }
    std::vector<uint8_t> out(total, 0);
    size_t so = 0, dO = 0;
    uint32_t mw = w, mh = h;
    for (uint32_t l = 0; l < levels; ++l) {
        const size_t sb = levelBytes(f, mw, mh);
        if (so + sb <= srcSize) decodeLevel(f, src + so, mw, mh, out.data() + dO);
        so += sb;
        dO += static_cast<size_t>(mw) * mh * 4;
        mw = std::max(1u, mw / 2);
        mh = std::max(1u, mh / 2);
    }
    return out;
}

#ifdef GVC_HAS_VULKAN
bool isBcFormat(VkFormat f) {
    return f == VK_FORMAT_BC1_RGB_UNORM_BLOCK || f == VK_FORMAT_BC1_RGBA_UNORM_BLOCK ||
           f == VK_FORMAT_BC2_UNORM_BLOCK || f == VK_FORMAT_BC3_UNORM_BLOCK;
}

size_t bcMipSize(VkFormat f, uint32_t w, uint32_t h) {
    return levelBytes(f == VK_FORMAT_BC1_RGB_UNORM_BLOCK ? BcFormat::BC1_RGB : f == VK_FORMAT_BC1_RGBA_UNORM_BLOCK
                          ? BcFormat::BC1_RGBA : f == VK_FORMAT_BC2_UNORM_BLOCK ? BcFormat::BC2 : BcFormat::BC3, w, h);
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
