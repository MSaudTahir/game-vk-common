# game-vk-common

Small C++ / Vulkan / GLSL helpers shared by the Vulkan game ports
([cod4mw](https://github.com/MSaudTahir/cod4mw), [nfsmw](https://github.com/MSaudTahir/nfsmw)).
Everything here is new, game-independent code; nothing ported from either game's
decompilation lives in this repo.

## Modules

| Path | What it is | Used by |
|---|---|---|
| `include/gvc/BcDecode.h`, `src/BcDecode.cpp` | CPU BC1/BC2/BC3 (DXT1/3/5) → RGBA8 decoder for GPUs without `textureCompressionBC` (Adreno / Mali, some MoltenVK). One implementation, two APIs: `BcFormat`-based (`decodeLevel` / `decodeChain`) and `VkFormat`-based (`decodeBc` / `bcMipSize` / `isBcFormat`). | nfsmw (was `src/BcDecode.*`), cod4mw (was `src/gfx_vulkan/BcDecode.*`) |
| `shaders/fxaa.glsl` | FXAA post pass (`gvc_fxaa(sampler2D, uv, level)`, levels 1–3), included by the post-combine shaders via `#include` (`glslangValidator -I`). | nfsmw `post_combine.frag`, cod4mw `post_composite.frag` |
| `tools/android-common.sh` | Sourceable helpers for the no-Gradle Android APK builds: SDK/NDK/build-tools discovery, Activity → classes.dex, debug keystore, zipalign + apksigner. | both `tools/build_android.sh` |

## Building / testing

```sh
cmake -S . -B build -DGVC_BUILD_TESTS=ON
cmake --build build && ctest --test-dir build
```

The tests are plain asserts (exit code = number of failures). nfsmw's
`tools/tests/bc_decode_pil.py` cross-check also runs against this decoder via the
test binary's `--decode` mode.

## Consuming it

Both games use this as a git submodule (`third_party/game-vk-common`) and add it
with `add_subdirectory`; the static library target is `gvc::gvc`. Shared shader
chunks are referenced through the submodule's `shaders/` directory, so a game's
shader compile step passes `-I third_party/game-vk-common/shaders`.

## License

MIT (see `LICENSE`). Game assets, decompilation-derived code and engine-specific
code stay in the game repos.
