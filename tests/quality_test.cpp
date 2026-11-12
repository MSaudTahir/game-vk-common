// Unit test of gvc's Quality (device tiers + preset tables).
//
// Build:  g++ -std=c++17 -O2 -Iinclude tests/quality_test.cpp src/Quality.cpp -o /tmp/gvc_quality_test
// Run:    /tmp/gvc_quality_test    -> exit code = number of failed checks

#include <cstdio>

#include "gvc/Quality.h"

using namespace gvc::quality;

static int g_fail = 0;
#define CHECK(c, ...) do { if (!(c)) { ++g_fail; std::printf("FAIL %s:%d: ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static bool near(float a, float b, float eps = 0.0001f) { return a >= b - eps && a <= b + eps; }

static DeviceInfo device(const char* gpu, bool mobile, uint32_t memMB = 0) {
    DeviceInfo d;
    d.gpuName = gpu;
    d.mobile = mobile;
    d.memoryMB = memMB;
    return d;
}

int main() {
    // --- tiers ---
    CHECK(suggestTier(device("Adreno (TM) 650", true)) == Tier::Mid, "Adreno 650 = Mid (the SD865 test phone)");
    CHECK(suggestTier(device("Adreno (TM) 530", true)) == Tier::Low, "Adreno 530 = Low");
    CHECK(suggestTier(device("Adreno (TM) 740", true)) == Tier::High, "Adreno 740 = High");
    CHECK(suggestTier(device("Mali-G77 MC9", true)) == Tier::Mid, "Mali-G77 = Mid");
    CHECK(suggestTier(device("Mali-G610 MC6", true)) == Tier::High, "Mali-G610 = High");
    CHECK(suggestTier(device("Mali-G52 MC2", true)) == Tier::Mid, "Mali-G52 = Mid");
    CHECK(suggestTier(device("Mali-G31", true)) == Tier::Low, "Mali-G31 = Low");
    CHECK(suggestTier(device("Mali-T880", true)) == Tier::Low, "Mali-T880 = Low (obsolete)");
    CHECK(suggestTier(device("Apple M2", true)) == Tier::High, "Apple = High");
    CHECK(suggestTier(device("NVIDIA GeForce RTX 3080", false, 16000)) == Tier::High, "desktop RTX = High");
    CHECK(suggestTier(device("AMD Radeon RX 6600", false, 8000)) == Tier::Mid, "desktop, 8 GB = Mid");
    CHECK(suggestTier(device("AMD Radeon RX 6600", false, 2000)) == Tier::Low, "desktop, 2 GB = Low");
    CHECK(suggestTier(device("Intel(R) UHD Graphics", false, 16000)) == Tier::Mid, "Intel iGPU = Mid");
    CHECK(suggestTier(device("Intel(R) UHD Graphics", false, 4000)) == Tier::Low, "Intel iGPU, 4 GB = Low");
    CHECK(suggestTier(device("Something exotic", true)) == Tier::Unknown, "unknown = Unknown");

    // --- suggested levels ---
    CHECK(suggestLevel(device("Adreno (TM) 650", true)) == Level::Medium, "SD865 phone = Medium");
    CHECK(suggestLevel(device("Adreno (TM) 740", true)) == Level::High, "flagship phone = High (never Ultra on mobile)");
    CHECK(suggestLevel(device("Mali-G31", true)) == Level::Low, "low-end phone = Low");
    CHECK(suggestLevel(device("NVIDIA GeForce RTX 3080", false, 32000)) == Level::Ultra, "big desktop = Ultra");
    CHECK(suggestLevel(device("Intel(R) UHD Graphics", false, 8000)) == Level::Medium, "mid desktop = Medium");
    CHECK(suggestLevel(device("Something exotic", true)) == Level::Low, "unknown mobile = conservative Low");
    CHECK(suggestLevel(device("Something exotic", false)) == Level::Medium, "unknown desktop = Medium");

    // --- presets ---
    {
        const Preset m = preset(Level::Medium, true);
        CHECK(near(m.resolutionScale, 0.67f), "mobile Medium scale matches the phone baseline: %f", m.resolutionScale);
        CHECK(m.mipBias == 1 && m.anisotropy == 2, "mobile Medium mip bias / aniso");
        CHECK(m.shadows == ShadowQuality::Off && m.aa == Aa::Low && m.targetFps == 60 && m.dynamicRes, "mobile Medium rest");
        const Preset l = preset(Level::Low, true);
        CHECK(near(l.resolutionScale, 0.5f) && l.targetFps == 30 && l.shadows == ShadowQuality::Off, "mobile Low");
        const Preset u = preset(Level::Ultra, false);
        CHECK(near(u.resolutionScale, 1.0f) && u.mipBias == 0 && u.anisotropy == 16, "desktop Ultra");
        CHECK(u.shadows == ShadowQuality::High && u.aa == Aa::High, "desktop Ultra rest");
        const Preset d = preset(Level::High, false);
        CHECK(near(d.resolutionScale, 1.0f) && d.anisotropy == 8 && d.shadows == ShadowQuality::Medium, "desktop High");
        const Preset dl = preset(Level::Low, false);
        CHECK(near(dl.resolutionScale, 0.75f) && dl.mipBias == 1 && dl.targetFps == 60, "desktop Low");
        CHECK(preset(Level::Custom, true).resolutionScale == preset(Level::Medium, true).resolutionScale, "Custom falls back to Medium");
    }

    // --- names ---
    CHECK(name(Level::Medium)[0] == 'M', "level name");
    CHECK(name(Tier::Mid) != nullptr && name(Tier::Mid)[0] == 'M', "tier name");

    std::printf(g_fail ? "%d checks FAILED\n" : "gvc quality_test: all checks passed\n", g_fail);
    return g_fail;
}
