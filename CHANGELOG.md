# Changelog

All notable changes to game-vk-common are documented here.
Versioning follows [Semantic Versioning](https://semver.org/): MAJOR.MINOR.PATCH.

## [1.0.0] - 2026-11-12

### Added
- `gvc::BcDecode` — CPU BC1/BC2/BC3 → RGBA8 decoder (dual API: `BcFormat` and `VkFormat`).
- `gvc::touch` — touch UI core: phase/pointer event model, virtual-stick math, button hit-testing (duck-typed), finger tracking, tap/hold bookkeeping, soft-disc overlay texture.
- `gvc::quality` — device-tier detection and per-level quality preset tables.
- `shaders/fxaa.glsl` — FXAA post pass (`gvc_fxaa`, levels 1–3).
- `tools/android-common.sh` — shared helpers for no-Gradle Android APK builds.

### Notes
- This is the first stable release. Consumers should pin to this tag (`v1.0.0`) or a specific commit.
- Breaking changes will be communicated via a major version bump and a migration note in this file.
