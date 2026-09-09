# v1.0 validation record

Release preparation: 2026-09-08. Hardware: NVIDIA GeForce RTX 5070, driver 616.64, Windows x64. These results describe this test machine, not every GPU, driver, Adobe version or footage type.

## Compiled release plug-in

The clean public source builds with genuine Adobe SDK interfaces, the pinned NVIDIA SDK and the identified release runtime. MSVC Release build completed without compiler warnings. All four CTest suites passed in 9.64 seconds:

| Suite | Coverage |
| --- | --- |
| Finishing controls | Neutral and zero-mix preservation; intensity; color/light/texture/detail and artifact behavior; selective blending; finite values; preset recipes. |
| Adobe host contract | Effect registration, metadata, persistent parameter IDs/types, groups and supervised preset changes, concurrent bypass/error calls and teardown. |
| Neural hardware | Actual enhanced pixels and preserved alpha/padding, 8/16/32-bit AE buffers, portrait and odd dimensions, SmartFX, legacy route, three effective styles, eight distinct recipes, individual control responses, shutdown/reinitialization. |
| Installer GPU preflight | Loads the packaged effect interface, renders an enhanced 180 × 225 frame and checks alpha and GPU cleanup. |

An initial restricted-sandbox GPU run timed out. The normal Windows run passed; release compatibility is based on the latter. The checker has an external 60-second limit so incompatible/hung runtimes cannot block setup indefinitely.

## Actual Adobe application exports

AE **26.3x87** exported the final neural/controls implementation during the private 1.2 build. Forty-four pixel comparisons passed: three styles differ, all eight recipes differ pairwise, neural processing changes source pixels, 16-/32-bit application renders complete, and wipe at 100 reproduces the original exactly. The public release retains that processing code, with package branding and a monotonic Adobe compatibility counter updated. The release binary is additionally exercised by the host/GPU harness above.

The user reported working, fast AE/Premiere operation for the preceding implementation. A complete Adobe-version matrix, additional physical GPU models and a broad real-footage benchmark have not been performed. Synthetic differences establish that controls work; they do not prove perceptual quality for every shot.

## Installer engine

Automated integration scenarios use marked isolated directories and the real payload. They do not modify the live Adobe installation or Windows uninstall registration:

- Full package validation plus the real GPU test, without installation.
- Concurrent setup is rejected before either installation can interfere with the other.
- Fresh install and exact module hash match.
- A deliberately locked maintenance file causes a commit failure; the previous plug-in and user-added file are restored.
- Successful upgrade preserves the previous folder outside the plug-in scan path.
- Corrupted payload and path-traversing manifest are rejected.
- A failing GPU checker leaves the existing install intact and preserves the diagnostic message.
- A hung checker is terminated after 60 seconds; the existing install stays intact.
- Uninstall removes owned unmodified files while retaining foreign/modified files and backups.

The EXE and ZIP use the same payload. The refreshed manual ZIP has no installer scripts; the EXE embeds its engine. Compiled-installer validation and isolated install/uninstall are performed before publication. Downloaded release assets are compared against local SHA-256 checksums.

An optional CI template covers CPU finishing tests and PowerShell parsing without proprietary SDKs. It is included as `.github/ci-template.yml`; GitHub automation is not enabled in the initial release because the publishing credential lacks workflow permission. The validation reported above was run locally.

## Development candidate 1.1.0 (2026-09-09)

Six local suites passed, including actual NVIDIA rendering, update JSON/version policy, cache/offline behavior, and cosmetic Adobe UI updates without parameter changes or forced rendering. The compiled installer passed validation, install, EXE/ZIP payload equality and uninstall checks. This is development validation, not a new published stable release.

The GitHub release workflow builds on a Windows hosted runner with pinned private SDK dependencies. It runs CPU/host/helper and packaging checks; GPU tests remain local because the hosted runner has no NVIDIA device. The deploy script requires a matching local debug-install receipt before requesting publication.

The first dispatched cloud build succeeded: https://github.com/Insider4444/4x4-Tools-DLSS-5/actions/runs/34315382735. It passed four hosted suites plus package-layout/hash/version verification; publication was deliberately disabled for this test. The private developer ZIP was extracted into a separate short folder, its manifest and dotfiles were verified, Git history was restored, and all six local build suites passed using only its bundled SDK/runtime paths. A deeply nested extraction hit MSVC's path limit; the final build script now reports that condition before compilation.
