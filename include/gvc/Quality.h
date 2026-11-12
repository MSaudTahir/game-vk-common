#pragma once

// Quality presets and device tiers for the mobile + desktop game ports.  One model every engine can
// hang its renderer knobs off: a hardware tier guess from a Vulkan GPU name / ids, and per-level
// preset tables (3D resolution scale, texture mip bias, anisotropy, shadow and AA quality, frame
// target, adaptive resolution).  Engine-specific knobs stay engine-side; these tables only seed them.
//
// The tier heuristics are deliberately simple string/number matching over the GPU name a Vulkan
// driver reports (VK_KHR_driver_properties' deviceName, or Android's ro.hardware.* properties) plus
// a RAM figure.  They err conservative on mobile and are easy to override by pinning a level.

#include <cstddef>
#include <cstdint>

namespace gvc {
namespace quality {

enum class Level : uint8_t { Low, Medium, High, Ultra, Custom, Count };
enum class Tier : uint8_t { Unknown, Low, Mid, High, Count };
enum class ShadowQuality : uint8_t { Off, Low, Medium, High, Count };
enum class Aa : uint8_t { Off, Low, Medium, High, Count }; // maps to an FXAA pass's levels 0..3

struct DeviceInfo {
    const char* gpuName = nullptr; // Vulkan deviceName, e.g. "Adreno (TM) 650", "Mali-G77 MC9", "NVIDIA GeForce RTX 3080"
    uint32_t vendorId = 0;         // VkPhysicalDeviceProperties::vendorID (0 = unknown)
    uint32_t deviceId = 0;
    uint32_t memoryMB = 0;         // total system RAM (0 = unknown)
    bool mobile = false;
    uint32_t screenW = 0, screenH = 0;
};

// The hardware class: Unknown keeps the level guess conservative (Low on mobile, Medium on desktop).
Tier suggestTier(const DeviceInfo& d);
// The default quality level for this device (mobile leans low: phones are thermally bound).
Level suggestLevel(const DeviceInfo& d);

struct Preset {
    float resolutionScale; // 3D render scale as a share of the screen (UI stays native)
    int mipBias;           // texture-quality drop, r_picmip style (0 = none)
    int anisotropy;        // max anisotropy for the samplers
    ShadowQuality shadows;
    Aa aa;                 // FXAA level (0 = off)
    int targetFps;         // 0 = uncapped
    bool dynamicRes;       // adaptive resolution scaling allowed
};

// The preset table for a level.  Mobile presets are a notch below desktop at the same level
// (resolution scale, shadows off, 60 fps cap) — the baseline the phone defaults were tuned to.
Preset preset(Level level, bool mobile);

const char* name(Level level); // "Low", "Medium", "High", "Ultra", "Custom"
const char* name(Tier tier);   // "Unknown", "Low", "Mid", "High"

} // namespace quality
} // namespace gvc
