# 4x4-Tools v1.0

First public release of **4x4-Tools DLSS 5 for Adobe After Effects and Premiere Pro**.

## Downloads

- **4x4-Tools-DLSS-5-v1.0-Setup.exe** — recommended guided Windows installer, upgrade backups and uninstaller.
- **4x4-Tools-DLSS-5-v1.0-Windows-x64.zip** — complete payload with PowerShell install, validate and uninstall scripts.
- **SHA256SUMS.txt** — checksums for both downloads.

Both packages include the same neural runtime. GitHub's automatic source archives are for developers and do not contain the runtime or compiled plug-in.

## Included

Three working neural styles, eight editable footage presets, intensity up to 200%, tone/structure controls, original color and lighting preservation, texture recovery, detail/artifact protection, color finishing, comparison views and feathered selective blending.

Setup verifies payload hashes and processes a small frame on the actual GPU before replacing files. It refuses incompatible systems, stops a hung checker after 60 seconds, preserves upgrade backups outside Adobe's scan path and rolls back a failed file replacement. Close both Adobe applications before installing.

## Start here

Reopen Adobe, search Effects for **4x4Tools-DLSS5**, and select **Natural balance** under Footage preset. Use the original/neural wipe to compare. Read the [controls guide](https://github.com/Insider4444/4x4-Tools-DLSS-5/blob/main/docs/CONTROLS.md), [installation guide](https://github.com/Insider4444/4x4-Tools-DLSS-5/blob/main/docs/INSTALLATION.md) and [compatibility matrix](https://github.com/Insider4444/4x4-Tools-DLSS-5/blob/main/docs/COMPATIBILITY.md).

## Requirements and limits

Windows x64, AE or Premiere Pro, and a compatible NVIDIA RTX GPU/driver. **RTX 5070 with driver 616.64 is locally verified.** Other RTX 30/40/50 systems are conditional on the included runtime passing preflight; some RTX 30 configurations need a different runtime that is not included. No universal generation-wide support is claimed.

This is an independent, unofficial community-runtime integration. The application is released as v1.0; neural rendering remains experimental. It performs same-resolution enhancement with an RGBA8 neural proxy, without real motion/depth guidance, upscaling or frame generation. HDR is experimental. Review faces, text, texture and motion before final export.

The installer is **unsigned** and may show a Windows unknown-publisher/SmartScreen prompt. Source code is MIT; NVIDIA components have separate terms and retain their own licenses. See [third-party notices](https://github.com/Insider4444/4x4-Tools-DLSS-5/blob/main/THIRD-PARTY-NOTICES.md).

## Validation

Four release build/test suites passed, including actual neural rendering. Forty-four comparisons of AE application exports confirm working styles/presets and exact original-wipe preservation. Installer integration covers validation, installation, upgrades, rollback, tamper/path rejection, GPU failure/timeout and removal. See the [validation record](https://github.com/Insider4444/4x4-Tools-DLSS-5/blob/main/docs/VALIDATION.md) for scope and limits.
