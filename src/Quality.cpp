#include "gvc/Quality.h"

#include <cstring>

namespace gvc {
namespace quality {

namespace {

bool has(const char* hay, const char* needle) {
    return hay && std::strstr(hay, needle) != nullptr;
}

// The last number in an Adreno name is the series ("Adreno (TM) 650" -> 650).
int adrenoSeries(const char* gpu) {
    int n = 0, last = -1;
    for (int i = 0; gpu[i]; ++i) {
        if (gpu[i] >= '0' && gpu[i] <= '9') {
            n = n * 10 + (gpu[i] - '0');
        } else if (n) {
            last = n;
            n = 0;
        }
    }
    if (n) last = n;
    return last;
}

// Mali's number is the generation after the 'G' ("Mali-G77 MC9" -> 77, "Mali-G52" -> 52).
int maliGeneration(const char* gpu) {
    const char* g = std::strstr(gpu, "Mali");
    if (!g) return -1;
    while (*g && (*g < '0' || *g > '9')) ++g;
    if (*g == '0' && g[1] >= '0' && g[1] <= '9') ++g; // "G05" style three-digit forms still read as 5x
    if (*g < '0' || *g > '9') return -1;
    int n = 0;
    while (*g >= '0' && *g <= '9') n = n * 10 + (*g++ - '0');
    return n;
}

} // namespace

Tier suggestTier(const DeviceInfo& d) {
    const char* gpu = d.gpuName;
    if (has(gpu, "Immortalis")) return Tier::High;
    if (has(gpu, "Adreno")) {
        const int s = adrenoSeries(gpu);
        if (s >= 700) return Tier::High;
        if (s >= 620) return Tier::Mid;
        if (s > 0) return Tier::Low;
        return Tier::Unknown;
    }
    if (has(gpu, "Mali")) {
        if (has(gpu, "Mali-T")) return Tier::Low; // the pre-Valhall T series, all long obsolete
        const int g = maliGeneration(gpu);
        if (g >= 90) return Tier::High;      // G9xx / Immortalis-class numbering
        if (g >= 70) return Tier::Mid;       // G7xx
        if (g >= 50) return Tier::Mid;       // G5x mid-range
        if (g > 0) return Tier::Low;
        return Tier::Unknown;
    }
    if (has(gpu, "Apple")) return Tier::High;
    if (has(gpu, "NVIDIA") || has(gpu, "GeForce") || has(gpu, "AMD") || has(gpu, "Radeon") ||
        has(gpu, "Arc") || has(gpu, "SwiftShader")) {
        // Desktop / hosted rasterizers: RAM is the only real gate here.
        if (d.memoryMB && d.memoryMB <= 4000) return Tier::Low;
        if (d.memoryMB && d.memoryMB <= 8000) return Tier::Mid;
        return Tier::High;
    }
    if (has(gpu, "Intel")) {
        // Intel iGPUs span everything; RAM is the best guess we get.
        if (d.memoryMB && d.memoryMB <= 4000) return Tier::Low;
        return Tier::Mid;
    }
    return Tier::Unknown;
}

Level suggestLevel(const DeviceInfo& d) {
    const Tier t = suggestTier(d);
    if (!d.mobile) {
        switch (t) {
            case Tier::Low: return Level::Low;
            case Tier::Mid: return Level::Medium;
            case Tier::High: return d.memoryMB && d.memoryMB >= 16000 ? Level::Ultra : Level::High;
            default: return Level::Medium;
        }
    }
    switch (t) {
        case Tier::High: return Level::High;
        case Tier::Mid: return Level::Medium;
        case Tier::Low: return Level::Low;
        default: return Level::Low; // unknown mobile hardware: conservative
    }
}

Preset preset(Level level, bool mobile) {
    if (mobile) {
        switch (level) {
            case Level::Low:
                return {0.50f, 2, 1, ShadowQuality::Off, Aa::Off, 30, true};
            case Level::High:
                return {0.80f, 0, 4, ShadowQuality::Low, Aa::Medium, 60, true};
            case Level::Ultra:
                return {1.00f, 0, 8, ShadowQuality::Medium, Aa::High, 0, false};
            default: // Medium
                return {0.67f, 1, 2, ShadowQuality::Off, Aa::Low, 60, true};
        }
    }
    switch (level) {
        case Level::Low:
            return {0.75f, 1, 2, ShadowQuality::Off, Aa::Low, 60, false};
        case Level::Medium:
            return {1.00f, 0, 4, ShadowQuality::Low, Aa::Medium, 0, false};
        case Level::Ultra:
            return {1.00f, 0, 16, ShadowQuality::High, Aa::High, 0, false};
        default: // High
            return {1.00f, 0, 8, ShadowQuality::Medium, Aa::High, 0, false};
    }
}

const char* name(Level level) {
    switch (level) {
        case Level::Low: return "Low";
        case Level::Medium: return "Medium";
        case Level::High: return "High";
        case Level::Ultra: return "Ultra";
        default: return "Custom";
    }
}

const char* name(Tier tier) {
    switch (tier) {
        case Tier::Low: return "Low";
        case Tier::Mid: return "Mid";
        case Tier::High: return "High";
        default: return "Unknown";
    }
}

} // namespace quality
} // namespace gvc
