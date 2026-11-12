#include "gvc/TouchUi.h"

#include <cmath>

namespace gvc {
namespace touch {

std::vector<uint8_t> circleTexture(unsigned size, float discAlpha, float rimInner, float rimOuter) {
    std::vector<uint8_t> px(static_cast<size_t>(size) * size * 4);
    const float half = static_cast<float>(size) * 0.5f;
    for (unsigned y = 0; y < size; ++y) {
        for (unsigned x = 0; x < size; ++x) {
            const float d = std::hypot(x + 0.5f - half, y + 0.5f - half) / half;
            float a;
            if (d < rimInner) a = discAlpha;
            else if (d < rimOuter) a = 1.0f;
            else if (d < 1.0f) a = (1.0f - d) / std::max(1.0f - rimOuter, 0.0001f);
            else a = 0.0f;
            uint8_t* o = &px[(static_cast<size_t>(y) * size + x) * 4];
            o[0] = o[1] = o[2] = 255;
            o[3] = static_cast<uint8_t>(std::clamp(a, 0.0f, 1.0f) * 255.0f);
        }
    }
    return px;
}

} // namespace touch
} // namespace gvc
