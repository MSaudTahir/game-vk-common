#pragma once

// CPU decoder for the DXT / S3TC (BC1, BC2, BC3) block-compressed textures old games ship, for
// GPUs and drivers without textureCompressionBC (Adreno / Mali phones, some MoltenVK devices).
// Output is tightly packed RGBA8.  Two APIs over one implementation:
//
//   * BcFormat / levelBytes / decodeLevel / decodeChain - single levels and mip chains
//   * VkFormat / isBcFormat / bcMipSize / decodeBc      - Vulkan formats, single levels
//
// BC1 "punch-through" (the 3 colour + transparent black mode, legal when color0 <= color1) is
// decoded for BcFormat::BC1_RGBA and VK_FORMAT_BC1_RGBA_UNORM_BLOCK; BcFormat::BC1_RGB /
// VK_FORMAT_BC1_RGB_UNORM_BLOCK forces the opaque 4 colour mode and alpha 255 (DXT1 RGB data
// must not use the transparent mode).

#include <cstddef>
#include <cstdint>
#include <vector>

#if defined(__has_include)
#if __has_include(<vulkan/vulkan.h>)
#include <vulkan/vulkan.h>
#define GVC_HAS_VULKAN 1
#endif
#endif

namespace gvc {

enum class BcFormat { BC1_RGB, BC1_RGBA, BC2, BC3 };

// Bytes of one tightly packed mip level (4x4 blocks: 8 bytes BC1, 16 bytes BC2 / BC3; sizes below
// 4 still take one block).
size_t levelBytes(BcFormat f, uint32_t w, uint32_t h);

// Decodes one level (w x h pixels, `src` holds levelBytes() bytes) to tightly packed RGBA8
// (w * h * 4 bytes in `dst`).
void decodeLevel(BcFormat f, const uint8_t* src, uint32_t w, uint32_t h, uint8_t* dst);

// Decodes `levels` mip levels stored one after the other (tightly packed, the layout the game
// texture containers use) into an RGBA8 chain of the same levels.  Levels that do not fit in
// `srcSize` come out as zeros.
std::vector<uint8_t> decodeChain(BcFormat f, uint32_t w, uint32_t h, uint32_t levels,
                                 const uint8_t* src, size_t srcSize);

#ifdef GVC_HAS_VULKAN
bool isBcFormat(VkFormat f);
// Size in bytes of one mip of a BC format.
size_t bcMipSize(VkFormat f, uint32_t w, uint32_t h);
// Decodes a w x h image of BC1/BC2/BC3 blocks at `src` into `dst` (w * h * 4 bytes).
void decodeBc(VkFormat f, const uint8_t* src, uint32_t w, uint32_t h, uint8_t* dst);
#endif

} // namespace gvc
