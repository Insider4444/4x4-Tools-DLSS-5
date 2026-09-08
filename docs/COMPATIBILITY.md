# Compatibility

## GPU and driver

| Hardware | v1.0 status |
| --- | --- |
| RTX 5070, Windows x64, NVIDIA driver 616.64 | Locally passed neural rendering, styles/presets, shutdown/reinitialization and installer preflight. |
| Other RTX 50 series | Not individually tested. Installation proceeds only after the bundled runtime passes a real rendering test. |
| RTX 40 series | Not individually tested. Community implementations report use of compatible modified runtimes; the bundled runtime must pass preflight on the user's card/driver. |
| RTX 30 series | Not individually tested or promised. Some community implementations require a different FP16 runtime. This release does not bundle that alternative. If this runtime fails, setup stops without replacing the existing plug-in. |
| Older RTX / RTX workstation models | Not certified. A recognized RTX card must still pass the same runtime test. |
| NVIDIA GTX, AMD, Intel, software rendering | Not supported by this neural runtime. |

Use a current NVIDIA driver suitable for your GPU. A card name alone does not establish model compatibility. The installer enumerates a NVIDIA Direct3D 12 adapter, loads the actual packaged Adobe effect and checks that a 180 × 225 frame is enhanced, alpha remains intact and GPU cleanup succeeds. It allows 60 seconds for the separate checker process. A driver crash, timeout, load failure or unchanged output prevents installation.

The test needs free GPU memory. Close demanding GPU applications before setup. Passing a small test does not guarantee that 4K/8K projects fit in VRAM or perform quickly. Multiple Adobe processes compete for GPU memory. No game hooks, registry overrides, driver changes or alternate model downloads are applied by this installer.

## Adobe applications

AE's effect API is used for both hosts. The build uses the Adobe 23.5 SDK interfaces. Local AE 26.3 application exports and the user's AE/Premiere workflow passed during development; every Adobe version has not been qualified. The host harness covers registration, parameter persistence, SmartFX, concurrent requests and the legacy rendering path.

AE buffers can be 8/16/32-bit, but neural processing uses an RGBA8 proxy. Premiere's legacy effect path supports ARGB8; this release does not implement Premiere's native GPU/32-bit pixel-format suite. Linear HDR is experimental. Test a short export in your project color pipeline before a full render.

## Community background

- [NVIDIA's DLSS 5 introduction](https://www.nvidia.com/en-us/geforce/news/dlss-5-3d-guided-neural-rendering/) describes a 3D-guided system. This plug-in has neither real depth nor motion vectors and must not be equated with that complete pipeline.
- [Resolve DLSS5 upstream](https://github.com/SAOG0721/DaVinci-Resolve-DLSS5) supplies the adapted Feature 18 backend and documents the experimental integration.
- [DLSS5-Autopilot runtime selection](https://github.com/Kizzuwatnaa/DLSS5-Autopilot/blob/main/core/sources.py) distinguishes architecture-specific runtime choices, including the Ampere FP16 case. These are community findings, not a compatibility certification for this Adobe package.

Compatibility statements were reviewed for this v1.0 release on 2026-09-08. File hashes, actual tests and reported failures take precedence over broad community claims.
