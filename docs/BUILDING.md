# Build and package

## Dependencies

Windows x64, Visual Studio 2022/2026 Desktop development with C++, a Windows SDK and CMake 3.24+ are required. Configure these separately obtained dependencies:

| Setting | Expected contents |
| --- | --- |
| AdobeSdk | After Effects SDK `Headers`, `Util`, `Resources/PiPLtool.exe` (or its `Examples` directory) |
| PhotoshopSdk | Photoshop 2026 C++ SDK `photoshopapi` headers; retain Adobe's SDK license |
| Lcms | Little CMS 2.19.1 source root with `src` and `include` |
| NgxSdk | NVIDIA DLSS SDK headers and `lib/Windows_x86_64/x64/nvsdk_ngx_s.lib` |
| Runtime | Reviewed community-modified `nvngx_dlssnr.dll`, pinned in `resources/runtime.json` |
| MakeNsis | NSIS 3.12 `makensis.exe` with its portable distribution |

The public repository contains no proprietary SDK development files. Keep dependencies in ignored `dependencies/` or outside the checkout. Runtime and SDK terms are separate from the source MIT license; see THIRD-PARTY-NOTICES.md.

## Build

The private portable kit already supplies relative dependency paths. Run `setup-dev.ps1` after extraction. For a normal checkout:

```powershell
.\setup-dev.ps1 -AdobeSdk 'D:\SDKs\AE\Examples' -PhotoshopSdk 'D:\SDKs\Photoshop\pluginsdk' -Lcms 'D:\SDKs\lcms2-2.19.1' -NgxSdk 'D:\SDKs\DLSS' -Runtime 'D:\Models\nvngx_dlssnr.dll' -MakeNsis 'D:\Tools\nsis-3.12\makensis.exe'
.\scripts\build.ps1
```

The default Release build creates both Adobe modules, the support/update helpers and nine test suites. Three suites require a real compatible NVIDIA GPU. Logs are in `build/*.log`; binaries are in `build/Release`. `build.ps1 -CpuOnly` runs the independent CPU tests without Adobe/NVIDIA SDKs. Use a short extraction path such as `C:\Dev` to avoid MSVC path limits.

The hosted release Action uses six CPU/host/helper suites; its Windows runner has no NVIDIA device. The private dependency commit is pinned in `.github/workflows/release.yml`. Runtime download URL, archive SHA-256, exact ZIP entry and binary SHA-256 are pinned in `resources/runtime.json`. Both hashes are checked before building. See [DEVELOPING.md](DEVELOPING.md) for GitHub deployment.

## Installer and manual ZIP

```powershell
.\publish-release.ps1
```

This builds/tests locally, packages the EXE and manual ZIP, and verifies the compiled installer without publishing. For packaging alone after a matching full build, use `scripts/package.ps1`. It refuses stale build receipts or an unreviewed runtime. Paths are recorded in `build/package-result.json`; artifacts use unique directories under `dist/`.

The public ZIP contains only `README.txt` and `4x4Tools-DLSS5/` with compiled modules, runtime, helpers, licenses and controls guides. It contains no source, build scripts or SDKs. The EXE embeds its installer engine and uses the identical payload.

Run `tests/installer_test.ps1 -PackageDir <package-dir> -IncludeTimeout` for isolated validation, install/upgrade/rollback, tamper rejection, timeout and removal scenarios. Run `tests/setup_exe_test.ps1 -SetupExe <exe> -Zip <zip>` for compiled extraction, actual GPU validation, both Adobe destinations, exact EXE/ZIP hashes and uninstall. `tests/package_layout_test.ps1` validates release layout/version/hashes. These tests use marked workspace directories and do not alter live Adobe folders or Windows uninstall registration.

Setup executes the actual AEX through SupportCheck.exe before displaying installation pages. A failed or timed-out 60-second neural check prevents installation and supplies a reason. This small test establishes that the runtime can execute on that PC; it cannot guarantee every image size or workload.

## Application verification

`tests/verify_ae_controls.jsx` is an opt-in AE script that refuses an existing project. Generate its fixture with `tests/compare_ae_controls.py --fixture`, run it in an empty AE session, then compare exports with `tests/compare_ae_controls.py <render-folder>`. It assumes English Best Settings/Lossless output templates.

`tests/verify_photoshop.jsx` runs in an empty Photoshop session, asks for an RGB fixture and a private output folder, then verifies filter availability and exports at 8/16/32-bit depth. It closes only the test images it opened. Automation can supply FOURX4_TEST_INPUT and FOURX4_TEST_OUTPUT on ExtendScript global scope. Photoshop invocation uses the scoped event UUID documented in [PHOTOSHOP.md](PHOTOSHOP.md). Validate RGB 8/16/32-bit paths, transparency, selection, presets, cancellation and a high-resolution image in the actual app before release. The native host harness additionally checks buffers, large coordinates and failure paths.

Keep projects, footage, diagnostics, build outputs and downloaded runtimes out of commits. Version comes from `release-config.json`; generated resources and installer metadata synchronize automatically. Preserve existing parameter IDs and the monotonic Adobe compatibility counter.
