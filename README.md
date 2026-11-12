# game-vk-common

A small library of C++ / Vulkan / GLSL building blocks shared across my Vulkan game projects,
present and future. Everything here is new, game-independent code; nothing ported from any
game's decompilation lives in this repo.

## Modules

| Path | What it is |
|---|---|
| `include/gvc/BcDecode.h`, `src/BcDecode.cpp` | CPU BC1/BC2/BC3 (DXT1/3/5) → RGBA8 decoder for GPUs and drivers without `textureCompressionBC` (Adreno / Mali, some MoltenVK). One implementation, two APIs: `BcFormat`-based (`decodeLevel` / `decodeChain`, mip chains) and `VkFormat`-based (`decodeBc` / `bcMipSize` / `isBcFormat`). |
| `include/gvc/TouchUi.h`, `src/TouchUi.cpp` | Touch UI core: the five-phase pointer event model, virtual-stick / steering / circle-to-square control math, virtual button layout + hit-testing (duck-typed over any button type), finger tracking, tap/hold bookkeeping, soft-disc overlay texture. Games keep their own action enums, layout tables, context policy and renderer glue. |
| `include/gvc/Quality.h`, `src/Quality.cpp` | Device tiers and quality presets: a hardware-tier guess from a Vulkan GPU name / ids / RAM, a default level per device, and per-level preset tables (3D resolution scale, mip bias, anisotropy, shadow + AA quality, frame target, adaptive resolution). Engines map the preset onto their own knobs. |
| `shaders/fxaa.glsl` | FXAA post pass (`gvc_fxaa(sampler2D, uv, level)`, levels 1–3), included by a project's post-combine shader via `#include` (`glslangValidator -I`). |
| `tools/android-common.sh` | Sourceable helpers for no-Gradle Android APK builds: SDK/NDK/build-tools discovery, Activity → classes.dex, debug keystore, zipalign + apksigner. |

## Building / testing

```sh
cmake -S . -B build -DGVC_BUILD_TESTS=ON
cmake --build build && ctest --test-dir build
```

The tests are plain asserts (exit code = number of failures); the decoder test's `--decode`
mode doubles as a cross-check target against Pillow (see the test header comment).

## Consuming it

Use it as a git submodule (`third_party/game-vk-common`) and add it with `add_subdirectory`;
the static library target is `gvc::gvc`. Shared shader chunks are referenced through the
submodule's `shaders/` directory, so a project's shader compile step passes
`-I third_party/game-vk-common/shaders`.

## License

MIT (see `LICENSE`). Game assets, decompilation-derived code and engine-specific code stay in
the game projects.
