# Changelog

## 1.2.0 — 2026-09-15

- One Windows suite for After Effects, Premiere Pro and the new Photoshop native filter.
- Photoshop RGB 8/16/32-bit images, ICC conversion, preserved transparency and dimensions, large-document coordinates and bounded overlapping tile processing.
- Crop preview, four control tabs, eight image presets, neural styles, intensity, restoration and subtle update status.
- Cross-generation community NVIDIA DLSS NR 310.8.SF-v2 runtime targeting RTX 20/30/40/50; RTX 5070 physically verified. Clear early installer rejection for unsupported hardware or failed runtime checks.
- Transactional installation and removal for both Adobe shared destinations, plus a complete manual-install ZIP.
- NVIDIA DLSS5 explanation, documented limits and six supplied before/after comparisons.
- GitHub builds both modules using pinned private SDK dependencies; portable private kit includes Photoshop and Little CMS dependencies.

## 1.1.0 — development candidate

- Subtle update status, daily background checks, opt-out and manual release-page action.
- Portable private developer kit and local debug-install workflow.
- deploy-main.ps1 triggers a GitHub Actions build and publication from committed source.
- Clean manual-install ZIP, tool-specific asset names and synchronized version configuration.


## v1.0 — 2026-09-08

First public 4x4-Tools release for Adobe After Effects and Premiere Pro.

- Three functional neural styles and eight editable footage presets.
- Intensity 0–200, tone, structure, automatic mask and natural restoration controls.
- Color finishing, comparison views and feathered selective blending.
- Correctly registered Adobe effect, SmartFX and Multi-Frame Rendering flags; lazy, serialized GPU use.
- Correct portrait readback allocation handling and safe explicit GPU teardown.
- Windows EXE and complete ZIP with bundled runtime, file verification, real GPU preflight, upgrade backups and removal support.
- Source, tests, build/packaging instructions, runtime provenance and separate third-party licenses.

Private development builds used versions 1.1.x/1.2.x. Public package and PE product version start at 1.0.0. Adobe's internal effect compatibility counter is 1.3.0 to stay monotonic for those existing projects. Original parameter IDs/types are retained; the formerly ineffective numbered hints now select actual neural styles, so older non-default settings may render differently. See the controls guide.
