#pragma once

// Touch UI core for Vulkan game ports: the pointer/phase event model window backends produce,
// backends produce, the pure control math (virtual stick, steering pad, the circle-to-square move
// stretch), virtual button layout + hit-testing, finger tracking, tap/hold bookkeeping and the
// shared soft-disc overlay texture.  What stays game-side: the button vocabulary (action enums),
// the layout tables' coordinates, context/visibility policy and the renderer glue.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace gvc {
namespace touch {

// --- Event model -----------------------------------------------------------
// The five-phase pointer model both Android / iOS window backends map their events into.

enum class Phase : uint8_t { Began, Moved, Stationary, Ended, Cancelled };

struct Pointer {
    int32_t id = -1;
    float x = 0.0f; // raw event coordinates (the caller's space)
    float y = 0.0f;
    Phase phase = Phase::Began;
};

// --- Pure control math -----------------------------------------------------

// The virtual stick: offset (dx, dy) from the stick's centre, full throw at `maxRadius`.
// Direction is kept, magnitude clamped into the disc, output scaled to -1..1 (outX = right,
// outY = down; negate for "forward").  A zero offset gives zero.
inline void joystickVector(float dx, float dy, float maxRadius, float& outX, float& outY) {
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len <= 0.0001f) {
        outX = outY = 0.0f;
        return;
    }
    const float clamped = std::min(len, maxRadius) / maxRadius;
    outX = (dx / len) * clamped;
    outY = (dy / len) * clamped;
}

// The thumb moves on a circle but old engines' move commands are read as a square: a full push at
// 45 degrees would give f = r = 0.71, so a forward diagonal ran at
// 71% speed and the speed dipped between forward and diagonal.  Stretch the circle onto the
// square (direction kept, magnitude = the push), like W+D on a keyboard.  Components in -1..1.
inline void circleToSquare(float& forward, float& right) {
    if (const float m = std::max(std::fabs(forward), std::fabs(right)); m > 0.0001f) {
        const float k = std::min(std::sqrt(forward * forward + right * right) / m, 1.41421356f);
        forward = std::clamp(forward * k, -1.0f, 1.0f);
        right = std::clamp(right * k, -1.0f, 1.0f);
    }
}

// 1D steering pad: full lock after `range` units of horizontal drag from where the thumb landed.
// A degenerate (zero) range steers nothing rather than snapping to full lock.
inline float steerScalar(float drag, float range) {
    if (range <= 0.0001f) return 0.0f;
    return std::clamp(drag / range, -1.0f, 1.0f);
}

// --- Virtual buttons -------------------------------------------------------

// The helpers below work with any button type exposing `.x` / `.y` / `.r` (edge-anchored x, y from
// the vertical centre (down = +), radius — in the game's layout unit; round by construction when
// the unit is uniform, e.g. FEng units with a fixed height of 480).  `gvc::touch::Button` below is
// one such type; games keep their own typed ones when they carry a typed action enum.

struct Button {
    int action = 0; // the game's action id (games with typed actions keep their own button type)
    const char* label = nullptr;
    float x = 0.0f, y = 0.0f, r = 0.0f;
    // Generic visibility flags; games that need more context states keep their own layout entries.
    bool freeRoamOnly = false; // shown only in free roam
    bool engageOnly = false;   // shown only while an Engage target is under the car
};

// Centre x of a button: `halfWidth` = half the screen width in the layout unit.
template <class B>
inline float buttonCentreX(const B& b, float halfWidth) {
    return b.x >= 0 ? -halfWidth + b.x : halfWidth + b.x;
}

// Circular hit test with `slack` around the drawn radius (a little slack makes thumbs forgiving).
template <class B>
inline bool buttonHit(const B& b, float centreX, float x, float y, float slack = 1.25f) {
    const float dx = x - centreX, dy = y - b.y;
    return dx * dx + dy * dy <= b.r * b.r * slack * slack;
}

// Nearest visible button under (x, y), or -1.  `visible` (optional) marks which buttons are shown.
template <class B>
inline int hitButton(const B* buttons, size_t count, float halfWidth, float x, float y,
                     float slack = 1.25f, const bool* visible = nullptr) {
    int best = -1;
    float bestD = 1e9f;
    for (size_t i = 0; i < count; ++i) {
        if (visible && !visible[i]) continue;
        const float d = std::hypot(x - buttonCentreX(buttons[i], halfWidth), y - buttons[i].y);
        if (d <= buttons[i].r * slack && d < bestD) {
            best = static_cast<int>(i);
            bestD = d;
        }
    }
    return best;
}

// --- Finger tracking -------------------------------------------------------

struct Finger {
    int32_t id = -1;      // -1 = free slot
    int32_t button = -1;  // index into the game's button table, -1 = none
    bool steering = false; // on the steering pad
    float x0 = 0.0f, y0 = 0.0f; // where the finger landed
    float x = 0.0f, y = 0.0f;
};

// Fixed-size slot tracker for multi-touch (the bookkeeping both overlays duplicate).  Slots are
// iterated directly (begin() .. end()); the game decides hit-testing, steering and per-slot policy.
class FingerTracker {
public:
    static constexpr size_t kMax = 10;

    Finger* begin() { return fingers_.data(); }
    Finger* end() { return fingers_.data() + kMax; }
    const Finger* begin() const { return fingers_.data(); }
    const Finger* end() const { return fingers_.data() + kMax; }
    Finger& at(size_t i) { return fingers_[i]; }

    Finger* byId(int32_t id) {
        if (id < 0) return nullptr;
        for (Finger& f : fingers_)
            if (f.id == id) return &f;
        return nullptr;
    }

    // The slot this pointer event belongs to.  Began claims a free slot for the id (a stale slot
    // with the same id is released first); other phases return the id's slot, or null.
    Finger* forPointer(int32_t id, Phase phase) {
        if (phase == Phase::Began) {
            if (Finger* f = byId(id)) release(*f);
            for (Finger& f : fingers_)
                if (f.id < 0) {
                    f.id = id;
                    return &f;
                }
            return nullptr;
        }
        return byId(id);
    }

    void release(Finger& f) { f = Finger{}; }

    void releaseAll() {
        for (Finger& f : fingers_) f = Finger{};
    }

private:
    std::array<Finger, kMax> fingers_{};
};

// --- Tap / hold bookkeeping ------------------------------------------------

// Per-action hold counts and sub-frame taps: a press marks both (so a tap shorter than a frame is
// still seen by the next poll); the consumer clears the taps with pollDone() once it has read them.
template <size_t N>
class TapHold {
public:
    void press(size_t action) {
        ++held_[action];
        tapped_[action] = true;
    }
    void unpress(size_t action) {
        if (held_[action] > 0) --held_[action];
    }
    void unpressAll() {
        held_.fill(0);
        tapped_.fill(false);
    }
    bool down(size_t action) const { return held_[action] > 0 || tapped_[action]; }
    int held(size_t action) const { return held_[action]; }
    void pollDone() { tapped_.fill(false); }

private:
    std::array<int, N> held_{};
    std::array<bool, N> tapped_{};
};

// --- Overlay texture -------------------------------------------------------

// The soft disc with a bright rim for touch overlays: white RGBA8 (the game tints per button at
// draw time).  `discAlpha` inside `rimInner`, full alpha through the rim, fading out by the edge.
// Defaults: a 30% disc with the bright rim between 86% and 97% of the radius.
std::vector<uint8_t> circleTexture(unsigned size = 128, float discAlpha = 0.30f,
                                   float rimInner = 0.86f, float rimOuter = 0.97f);

} // namespace touch
} // namespace gvc
