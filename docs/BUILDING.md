# Build and package

## Dependencies

- Windows x64; Visual Studio 2022/2026 C++ desktop Build Tools and a Windows SDK; CMake 3.24 or later.
- A separately obtained Adobe After Effects SDK containing `Headers`, `Util` and `Resources/PiPLtool.exe`. v1.0 was built with the 23.5 interfaces (SDK 13.29). The script accepts either that directory or an SDK root with `Examples` beneath it.
- NVIDIA DLSS SDK, build tested at commit `a291cc7d2cc642a51566f3dfd5376f635cd1b284`, containing `include` and `lib/Windows_x86_64/x64/nvsdk_ngx_s.lib`.
- A compatible, separately obtained `nvngx_dlssnr.dll`. The published v1.0 runtime hash/provenance is in THIRD-PARTY-NOTICES.md. Obtain applicable rights before redistributing any SDK or runtime material.

The repository deliberately contains no proprietary SDK development files. Put dependencies outside the repository or in the ignored `dependencies` directory. Do not publish SDK headers, development libraries or tools with source commits.

## Full build

From the repository root in Windows PowerShell:

```powershell
.\scripts\build.ps1 -AdobeSdk 'D:\SDKs\AfterEffects\Examples' -NgxSdk 'D:\SDKs\DLSS' -Runtime 'D:\Models\nvngx_dlssnr.dll'
```

Environment alternatives: `AE_SDK_BASE_PATH`, `DLSS_SDK_ROOT`, `DLSSNR_RUNTIME_DLL`. The script discovers MSVC/CMake, builds Release with the static C++ runtime and runs four test suites. Output is `build/Release`; logs are `build/*.log`. Full tests require native access to the NVIDIA driver. A restricted process sandbox can make GPU initialization or teardown stall; it is not a substitute for a normal Windows hardware run.

To run only the CPU controls tests without proprietary SDKs or NVIDIA hardware:

```powershell
.\scripts\build.ps1 -CpuOnly
```

Or configure CMake with `-DBUILD_ADOBE_PLUGIN=OFF`. The optional `.github/ci-template.yml` configuration covers the CPU path and script parsing; it does not test Adobe or NVIDIA hardware. To enable GitHub Actions, an account/token with workflow publishing permission can place the template at `.github/workflows/ci.yml`. Automation is not enabled by the initial release.

## Installer and ZIP

Use the official [NSIS 3.12 portable distribution](https://sourceforge.net/projects/nsis/files/NSIS%203/3.12/nsis-3.12.zip/download). The reviewed archive SHA-256 is `56581F90DB321581C5381193D796FFFCF2D24B2F8FED2160A6C6A3BAA67F2C4F`. Keep it outside the source repository. Branding assets are committed; regenerate them with Python 3 and `scripts/make_branding.py` if changed.

```powershell
.\scripts\package.ps1 -MakeNsis 'D:\BuildTools\nsis-3.12\makensis.exe'
```

The script builds a private staging package, creates a manifest with SHA-256 for every payload file, compiles setup and creates the ZIP plus SHA256SUMS.txt in `dist`. It refuses to overwrite existing release artifacts. It also refuses a runtime that does not match the reviewed v1.0 hash. Update notices, distribution authorization and hardware validation before changing that pin.

Run `tests/installer_test.ps1 -PackageDir <staging-package>` to exercise validation, installation, upgrade backups, uninstall and tamper/path rejection; add `-IncludeTimeout` for the 60-second hung-checker scenario. Run `tests/setup_exe_test.ps1 -SetupExe <exe-path> -Zip <zip-path>` to test the compiled installer, compare EXE/ZIP payloads and exercise the self-removing uninstaller. These use isolated marked directories and do not alter Adobe's installed plug-in or Windows uninstall registration. The compiled test keeps its child at the caller's privilege level because no elevated writes are needed in the marked folder. Developer validation can also use `/S /VALIDATEONLY`; isolated installation uses `/TESTROOT=<marked-local-test-directory>`. These switches do not certify actual host performance.

The full build also produces `SupportCheck.exe`, which loads the actual adjacent `.aex` and its `runtime` directory. It reports one JSON result and a process exit code. The installer runs it as a child with a 60-second timeout and local diagnostics. Installation never continues after a failed check.

## Application verification

`tests/verify_ae_controls.jsx` is an opt-in AE script that refuses to touch an existing project. Generate the synthetic fixture with `tests/compare_ae_controls.py --fixture`, then run the JSX in an empty AE session with file-writing scripts enabled. It renders styles/presets and 8/16/32-bit paths into ignored `artifacts` folders. Compare its exports with `tests/compare_ae_controls.py <render-folder>`. The script assumes English Best Settings/Lossless output templates and does not run automatically in CI.

Keep application projects, footage, diagnostics, build outputs and downloaded runtimes out of commits. GitHub's automatic source archives contain only the public repository; users should download the explicit Windows release assets for installation.

## Versioning

Public version, installer, manifest and Windows file resources are 1.0.0; release tag is `v1.0`. Adobe's `PF_VERSION`/PiPL counter is 1.3.0 to follow private 1.1/1.2 builds. Keep the counter monotonic and preserve parameter disk IDs when updating existing projects.
