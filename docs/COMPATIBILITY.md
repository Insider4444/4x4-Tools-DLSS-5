# Compatibility — v1.2 suite

Windows x64, Direct3D 12 and an NVIDIA RTX GPU are required. The suite includes
the community **DLSS NR 310.8.SF-v2** runtime, selected to extend compatibility
beyond the original RTX 50 implementation. Setup tests the actual packaged
engine before displaying its installation pages.

| GPU | Evidence and expected behavior |
| --- | --- |
| RTX 5070, driver 616.64 | Locally verified: AE neural rendering, styles, cleanup and tiled Photoshop 16-bit processing. |
| Other RTX 50 cards | Community runtime target; each installation must pass its own render test. Not individually tested here. |
| RTX 40 cards | Community runtime target. Not physically tested by this project; card, driver and free VRAM still matter. |
| RTX 30 and RTX 20 cards | Community cross-generation target. FP16 paths may be substantially slower. Use smaller Photoshop tiles. Not physically tested by this project. |
| RTX workstation / laptop cards | An RTX adapter may pass the same check; these models are not individually qualified. Power and VRAM limits affect performance. |
| GTX, AMD, Intel, CPU-only, macOS | No processing backend in this release. Windows setup stops with the reason if it cannot find a usable RTX adapter. |

This is a tested RTX 5070 build with broader community runtime coverage, **not
a certification of every RTX card**. Community game-mod results cannot prove
that an Adobe workflow will work.

## What setup checks

Setup verifies every payload hash, selects a high-performance NVIDIA RTX
Direct3D 12 adapter, loads the packaged effect and renders a 180 × 225 frame.
The image must change, alpha must remain intact, and GPU cleanup must succeed.
The separate checker has a 60-second timeout. Missing files, load errors,
runtime failures, invalid output and timeouts produce a specific error before
any Adobe files are replaced. Detailed logs remain available.

The checker and engine use the same adapter preference, avoiding a GTX ahead
of an RTX in mixed-GPU systems. No drivers, game hooks or registry compatibility
overrides are installed.

Passing this small test establishes basic runtime operation. It cannot guarantee
that an 8K video frame fits in VRAM or that a large Photoshop image finishes
quickly. Photoshop's Low VRAM option limits tile size; CPU processing uses a
512 MiB tile budget plus host, ICC and GPU allocations.

## Adobe hosts

| Host | Processing route and limits |
| --- | --- |
| After Effects | Adobe effect / SmartFX; 8/16/32-bit host buffers and Multi-Frame Rendering registration. GPU work is serialized per process. Existing parameter IDs remain stable. |
| Premiere Pro | Legacy Adobe effect route, ARGB8 host processing. Native Premiere GPU / 32-bit pixel-format suites are not implemented. |
| Photoshop 2026+ on Windows | Native .8bf filter, JSON PiPL registration, RGB 8/16/32-bit, large-document coordinates and overlapping tiles. CMYK, indexed color and individual-channel filtering are unavailable. |

The model uses an 8-bit proxy. Photoshop preserves finer original information
through float residual reconstruction; this is not native 16/32-bit model
inference. HDR is experimental. ICC conversion retains the document RGB profile;
32-bit images require a matrix RGB profile. Dimensions and transparency stay
unchanged. See [Photoshop guide](PHOTOSHOP.md).

## NVIDIA DLSS 5 and this integration

[NVIDIA's DLSS 5 documentation](https://www.nvidia.com/en-us/geforce/news/dlss-5-3d-guided-neural-rendering/)
describes 3D-guided neural rendering integrated with game data. This independent
Adobe adaptation uses existing images and video: it receives no real scene depth
or engine motion vectors and resets neural history per frame. It cannot promise
the temporal consistency of an integrated game pipeline. It performs enhancement
at the input resolution, not upscaling or frame generation. Review motion, text,
faces and fine patterns before delivery.

Research checked for this release on 2026-09-10:

- [Resolve DLSS5 upstream](https://github.com/SAOG0721/DaVinci-Resolve-DLSS5): origin of the adapted Feature 18 engine.
- [DLSS5Kit architecture table](https://github.com/UgurInanc12/DLSS5Kit#what-it-verifies-rather-than-assumes): identifies SF-v2 for RTX 20–50 and explains machine code versus portable PTX.
- [OptiScaler DLSS NR multipass project](https://github.com/wilsjo2/OptiScaler-DLSSNR-PreSR-Multipass): cross-generation runtime paths and older-generation performance differences.
- [Exact runtime release](https://github.com/RankFTW/rhi-repo/releases/tag/dlssnr-310.8.SF-v2): archive and binary hashes are pinned in [runtime.json](../resources/runtime.json).

These are implementation references and community findings, not NVIDIA
endorsement or a substitute for the actual render check.