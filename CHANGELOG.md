# Changelog

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
