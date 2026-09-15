# 4x4Tools-DLSS5-win v1.2.0 — Adobe suite

NVIDIA-based DLSS 5 neural enhancement now includes **Photoshop**, alongside
After Effects and Premiere Pro, in one Windows installer and manual-install ZIP.

## New in v1.2

- Native Photoshop .8bf filter for RGB 8/16/32-bit images: high-resolution
  overlapping tiles, large-document coordinates, ICC conversion, transparency
  preservation and staged output. Original dimensions remain unchanged.
- Crop preview, Original toggle, eight presets and Neural, Restore, Color and
  Compare controls. Select smaller tiles for lower VRAM usage.
- Community **310.8.SF-v2** runtime targeting RTX 20/30/40/50 generations.
  Physically verified on RTX 5070 / driver 616.64; other cards must pass setup's
  render check. Older GPUs may be substantially slower.
- Compatibility checks before the installer pages, with the specific failure
  reason when the GPU, driver or package cannot run the engine.
- Suite upgrade/uninstall transactions, backups and rollback covering both
  AE/Premiere and Photoshop folders.
- Subtle update notices, optional daily checks, manual checks and release links.
- Portable private development kit and GitHub Actions builds triggered by
  deploy-main.ps1, with release-config.json and Markdown update-note.md.
- [Six before/after comparisons](https://insider4444.github.io/4x4-Tools-DLSS-5/gallery/)
  and clearer guides explaining the NVIDIA runtime, creative controls and limits.

## Install

Save and close Adobe applications, then run the EXE. For manual installation,
extract the ZIP and follow README.txt. Releases contain only the installer,
installable plug-in ZIP and checksums; no SDKs or build scripts are inside the ZIP.

Photoshop: **Filter → 4x4Tools → DLSS5 - Image Enhancement** on an RGB image in
Photoshop 2026+. AE/Premiere: **4x4Tools-DLSS5** in Effects.

## Scope

This independent integration uses a modified NVIDIA runtime. It is not an
official NVIDIA or Adobe product and receives no game motion/depth data.
No upscaling or frame generation is performed. The model uses an 8-bit proxy;
Photoshop preserves finer original information via float residual reconstruction.
HDR is experimental. Review faces, text, patterns and motion before delivery.

GTX, AMD, Intel, CPU-only and macOS processing are unavailable.
See [compatibility](https://github.com/Insider4444/4x4-Tools-DLSS-5/blob/main/docs/COMPATIBILITY.md)
and the [validation record](https://github.com/Insider4444/4x4-Tools-DLSS-5/blob/main/docs/VALIDATION.md).