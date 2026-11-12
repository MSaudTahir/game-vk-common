// Unit test of gvc's TouchUi (shared touch core: control math, button hit-testing, finger
// tracking, tap/hold bookkeeping, disc texture).
//
// Build:  g++ -std=c++17 -O2 -Iinclude tests/touch_ui_test.cpp src/TouchUi.cpp -o /tmp/gvc_touch_test
// Run:    /tmp/gvc_touch_test    -> exit code = number of failed checks

#include <cmath>
#include <cstdio>

#include "gvc/TouchUi.h"

using namespace gvc::touch;

static int g_fail = 0;
#define CHECK(c, ...) do { if (!(c)) { ++g_fail; std::printf("FAIL %s:%d: ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static bool near(float a, float b, float eps = 0.001f) { return std::fabs(a - b) <= eps; }

int main() {
    // --- joystickVector: direction kept, magnitude clamped, output -1..1 ---
    {
        float x, y;
        joystickVector(3.0f, 4.0f, 10.0f, x, y);   // len 5 inside the disc: scaled by 5/10
        CHECK(near(x, 0.3f) && near(y, 0.4f), "inside the disc: %f %f", x, y);
        joystickVector(30.0f, 40.0f, 10.0f, x, y); // len 50 outside: clamped to the rim, direction kept
        CHECK(near(x, 0.6f) && near(y, 0.8f), "clamped to the rim: %f %f", x, y);
        joystickVector(0.0f, 0.0f, 10.0f, x, y);
        CHECK(x == 0.0f && y == 0.0f, "zero offset is zero");
        joystickVector(1e-9f, 0.0f, 10.0f, x, y);
        CHECK(x == 0.0f && y == 0.0f, "epsilon offset is zero");
        joystickVector(0.0f, -5.0f, 5.0f, x, y);   // full throw up
        CHECK(x == 0.0f && near(y, -1.0f), "full throw: %f %f", x, y);
    }
    // --- circleToSquare: a full 45 degree push reaches the square's corner at full magnitude ---
    {
        float f = 0.70710678f, r = 0.70710678f;
        circleToSquare(f, r);
        CHECK(near(f, 1.0f) && near(r, 1.0f), "full diagonal: %f %f", f, r);
        float a = 0.5f, b = 0.5f;   // len 0.707 at 45 deg: k = 1.414 -> magnitude kept, at the corner direction
        circleToSquare(a, b);
        CHECK(near(a, 0.7071f, 0.001f) && near(b, 0.7071f, 0.001f), "half diagonal keeps magnitude: %f %f", a, b);
        float c = 1.0f, d = 0.0f;   // straight push is untouched
        circleToSquare(c, d);
        CHECK(c == 1.0f && d == 0.0f, "straight push untouched");
        float e = 0.0f, g = 0.0f;
        circleToSquare(e, g);
        CHECK(e == 0.0f && g == 0.0f, "zero untouched");
        float h = 2.0f, i = 0.0f;   // out-of-range input clamped back into -1..1
        circleToSquare(h, i);
        CHECK(h == 1.0f, "clamped to 1");
    }
    // --- steerScalar ---
    CHECK(near(steerScalar(30.0f, 60.0f), 0.5f), "half lock");
    CHECK(near(steerScalar(120.0f, 60.0f), 1.0f), "full lock clamps");
    CHECK(near(steerScalar(-120.0f, 60.0f), -1.0f), "full lock the other way");
    CHECK(steerScalar(10.0f, 0.0f) == 0.0f, "zero range is safe");

    // --- buttons: edge-anchored centre + nearest hit with slack and a visibility mask ---
    {
        Button buttons[3];
        buttons[0] = {1, "A", -80, 160, 52};   // 80 from the right edge
        buttons[1] = {2, "B", 40, -206, 24};   // 40 from the left edge
        buttons[2] = {3, "C", -300, 0, 30};
        const float half = 640.0f * 0.5f;
        CHECK(near(buttonCentreX(buttons[0], half), 320.0f - 80.0f), "right-anchored");
        CHECK(near(buttonCentreX(buttons[1], half), -320.0f + 40.0f), "left-anchored");
        CHECK(hitButton(buttons, 3, half, 240.0f, 160.0f) == 0, "hits button 0");
        CHECK(hitButton(buttons, 3, half, -280.0f, -206.0f) == 1, "hits button 1");
        CHECK(hitButton(buttons, 3, half, 1000.0f, 1000.0f) == -1, "no hit");
        // nearest wins: right between 0 and 2, but 2's slack bubble is closer
        CHECK(hitButton(buttons, 3, half, 100.0f, 40.0f) == -1, "between the two right-strip buttons, no hit");
        buttons[2].x = -140; buttons[2].y = 160; // overlapping slack bubbles with button 0, centre (180, 160)
        CHECK(hitButton(buttons, 3, half, 205.0f, 160.0f) == 2, "nearest wins");
        const bool visible[3] = {true, false, true};
        CHECK(hitButton(buttons, 3, half, -280.0f, -206.0f, 1.25f, visible) == -1, "invisible button not hit");
    }

    // --- FingerTracker: claim, reuse, stale id, release ---
    {
        FingerTracker t;
        Finger* a = t.forPointer(5, Phase::Began);
        CHECK(a && a->id == 5, "Began claims a slot");
        CHECK(t.forPointer(5, Phase::Moved) == a, "Moved finds the same slot");
        Finger* b = t.forPointer(7, Phase::Began);
        CHECK(b && b != a, "a second pointer gets its own slot");
        // a stale slot with the same id (missed Ended) is released and re-claimed
        a->steering = true;
        Finger* a2 = t.forPointer(5, Phase::Began);
        CHECK(a2 && a2->id == 5 && !a2->steering && a2->button == -1, "stale slot cleared on re-Began");
        t.release(*a2);
        CHECK(t.byId(5) == nullptr, "released id gone");
        CHECK(t.forPointer(99, Phase::Ended) == nullptr, "unknown id on Moved/Ended is null");
        for (Finger& f : t) { (void)f; }
        t.releaseAll();
        for (const Finger& f : t) CHECK(f.id == -1, "releaseAll frees every slot");
        // saturate: all 10 slots taken, an 11th Began returns null
        for (int i = 0; i < 10; ++i) CHECK(t.forPointer(100 + i, Phase::Began) != nullptr, "slot %d claimed", i);
        CHECK(t.forPointer(200, Phase::Began) == nullptr, "no free slot left");
    }

    // --- TapHold ---
    {
        TapHold<4> th;
        th.press(2);
        CHECK(th.down(2), "press registers");
        CHECK(th.held(2) == 1, "one hold");
        CHECK(!th.down(0), "other actions untouched");
        th.press(2);
        CHECK(th.held(2) == 2, "two fingers held");
        th.unpress(2);
        CHECK(th.down(2) && th.held(2) == 1, "still held by the second finger");
        th.unpress(2);
        CHECK(th.down(2), "the lift still counts as a tap until pollDone");
        th.pollDone();
        CHECK(!th.down(2), "cleared by pollDone");
        th.press(1);
        th.unpress(1);
        th.pollDone();
        CHECK(!th.down(1), "sub-frame tap cleared by pollDone");
        th.press(0);
        th.unpressAll();
        CHECK(!th.down(0), "unpressAll clears");
    }

    // --- circleTexture ---
    // Pixel (x, y) has d = hypot(x + 0.5 - half, y + 0.5 - half) / half, so the centre is (63, 63)
    // and (63, 63 + k) has d = k / 64 on a 128 texture.
    {
        const auto px = circleTexture(128);
        CHECK(px.size() == 128u * 128 * 4, "size");
        auto alpha = [&](int x, int y) { return px[static_cast<size_t>((y * 128 + x) * 4) + 3]; };
        CHECK(px[0] == 255 && px[1] == 255 && px[2] == 255, "white rgb");
        CHECK(alpha(63, 63) == 76, "disc alpha 0.30: %d", alpha(63, 63));
        CHECK(alpha(0, 0) == 0, "corner (d = 1.40) transparent");
        CHECK(alpha(63, 63 + 54) == 76, "still the disc at d = 0.836");
        CHECK(alpha(63, 63 + 56) == 255, "rim full alpha at d = 0.867");
        CHECK(alpha(63, 63 + 61) == 255, "rim still full at d = 0.945");
        const int fade = alpha(63, 126);       // d = 0.9766: (1 - d) / 0.03 = 0.780
        CHECK(fade == 198, "rim fade near the edge: %d", fade);
        const auto small = circleTexture(64, 0.5f, 0.5f, 0.9f);
        CHECK(small.size() == 64u * 64 * 4, "custom size");
        auto alpha2 = [&](int x, int y) { return small[static_cast<size_t>((y * 64 + x) * 4) + 3]; };
        CHECK(alpha2(31, 31) == 127, "custom disc alpha 0.5: %d", alpha2(31, 31));
        CHECK(alpha2(31, 51) == 255, "custom rim at d = 0.609");
    }

    std::printf(g_fail ? "%d checks FAILED\n" : "gvc touch_ui_test: all checks passed\n", g_fail);
    return g_fail;
}
