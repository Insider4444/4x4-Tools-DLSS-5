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

The EXE/ZIP downloaded from that GitHub run also passed real GPU validation, isolated installation, payload equality and self-removing uninstall on the RTX 5070. Installer source comparison normalizes Git's CRLF/LF checkout difference; all installed binary hashes remain exact.

## Suite v1.2.0 local validation (2026-09-15)

Windows x64, RTX 5070, NVIDIA driver 616.64; community runtime 310.8.SF-v2 with the exact binary/archive pins in resources/runtime.json. This replaces the v1.0 runtime. RTX 20/30/40/50 coverage outside this physical 5070 is based on community runtime targets, not testing each card.

All nine Release CTest suites passed after the Photoshop preview repaint fix (14.97 seconds): finishing controls, update policy, Photoshop high-resolution tiling, Adobe host contract, Adobe neural hardware, installer GPU preflight, update helper, Photoshop host contract and Photoshop neural hardware. High-resolution tests cover every pixel of an 8K image, 40,001-pixel panoramas, 300,000-coordinate planning, overlap corners, document-space finishing, memory guards and cancellation. Host tests check 8/16/32-bit channel layouts, untouched alpha/padding, selections and processing/read failures.

Actual Photoshop 27.11.0 (Beta) executed the registered filter on RGB 16-bit images: 1089 x 613 with a 512-pixel tile core (3.3 seconds), and 8192 x 4320 with a 1024-pixel core (30.3 seconds). Dimensions and depth were unchanged; processed histograms differed. Timings include saving the private PNG test output. The 8K fixture was a resized copy of a provided 4K source, used to test processing capacity; it is not evidence of neural upscaling or an 8K quality benchmark. The scoped UUID, rather than the raw four-character terminology ID, is required for Photoshop scripting.

Six supplied before/after pairs were visually checked. The gallery loaded all 12 image files, switched all six scenes, passed keyboard Home/End endpoints, and fit a 390-pixel viewport without page overflow. These supplied comparisons have unknown generation settings/version and are not new-filter benchmarks.

Earlier AE application comparisons above describe the preceding video implementation. v1.2 additionally passes the AE host/GPU regression harness; no new broad Adobe-version or real-footage certification is claimed.

The final local EXE/ZIP passed all ten installer scenarios: concurrent setup rejection, real GPU validation, fresh install, rollback on failed commit, upgrade backups, tampered files, path traversal, unsupported GPU, a 60-second hung checker, and uninstall preserving foreign/modified files. The compiled EXE separately passed extraction/preflight, installation to both marked Adobe destinations, exact EXE/ZIP payload hashes and self-removing uninstall. No live Adobe folders were changed by these isolated tests.

Photoshop dialog verification passed: a full-size crop rendered, Original switched the displayed pixels, selecting Strong enhancement changed intensity from 75 to 125 and updated style/tone/structure, all four tabs exposed their controls, and Apply completed. Owner-drawn preview painting keeps the cached crop visible across dialog redraws. Actual RGB8 selection/transparency exports were compared pixel-for-pixel: alpha and pixels outside the selection were exact, while RGB inside the selection changed.

### Final verification (2026-09-16)

After removing a redundant depth expression from Photoshop's PiPL enablement rule, the installed filter passed the actual Photoshop 27.11.0 application test for RGB8, RGB16 and RGB32. Each 1089 x 613 document processed with a 512-pixel tile core and retained its dimensions and depth; outputs saved as PNG8, PNG16 and floating-point TIFF respectively. The repeatable opt-in script is `tests/verify_photoshop.jsx`.

The final-source GitHub build succeeded: https://github.com/Insider4444/4x4-Tools-DLSS-5/actions/runs/34964924065. Its downloaded installer and ZIP passed real GPU preflight, isolated installation to both Adobe destinations, exact payload equality and self-removing uninstall.

The private developer archive passed verification of all 2,054 manifest files. A separate extraction restored its bundled Git history and rebuilt using bundled dependency paths; all nine local suites passed in 16.18 seconds. The matching local debug build was installed and verified in both Adobe shared plug-in destinations.
