# Third-party notices

## 4x4-Tools plug-in and installer source

Copyright (c) 2026 4x4-Tools and contributors. MIT; see LICENSE. This license covers original project code, not third-party SDKs or model binaries.

## Neural backend

Adapted from SAOG0721/DaVinci-Resolve-DLSS5, commit `a659e5c674388ea8026f4cf8df9f206826d12452`, MIT. Copyright notice and license are in licenses/UPSTREAM-MIT.txt and src/neural/UPSTREAM-LICENSE.txt. Adobe adaptations and provenance are recorded in src/neural/PROVENANCE.txt.

## NVIDIA components

**This software contains source code provided by NVIDIA Corporation.** The compiled plug-in links the NVIDIA NGX SDK. The build used NVIDIA/DLSS commit `a291cc7d2cc642a51566f3dfd5376f635cd1b284`. The SDK's license is included in licenses/NVIDIA-RTX-SDK.txt and each binary package. SDK source, headers and standalone libraries are not distributed in this repository.

The v1.2 suite contains a separately licensed, community-modified `runtime/nvngx_dlssnr.dll`, the ShortFuse cross-generation build **310.8.SF-v2**, obtained from the [RankFTW/rhi-repo community mirror](https://github.com/RankFTW/rhi-repo/releases/tag/dlssnr-310.8.SF-v2). Its file version is 310.8.0.0 and SHA-256 is:

`6EB209E764F39872625DEBD6ABAF45E2BB6322F6F270F781F70C059AE30B3927`

The downloaded archive SHA-256 is `1DA35941894994EB087E017577829E492454E9BAE3A6A9397027069CEB74955C`. Both hashes and the exact download URL are pinned in `resources/runtime.json`. The earlier v1.0 runtime from Resolve Experimental v0.3.1 had SHA-256 `984BEE0F775C277D5829B8FD6775D53A7B0F75396C852B3AAF06A18375F81014`.

This is not represented as an untouched official NVIDIA release. The publisher has confirmed permission to redistribute the supplied binary with this application. That confirmation does not relicense the binary as MIT, grant SDK source redistribution rights, or establish support on every GPU. NVIDIA components retain their applicable terms and ownership. Downstream distributors must obtain and satisfy the rights and obligations applicable to their distribution; this repository grants no additional NVIDIA rights.

## Adobe SDK

Adobe's After Effects SDK supplies build-time interfaces and PiPL tooling. The Photoshop filter uses the Windows Photoshop SDK 2026 v2 interfaces and JSON PiPL format. Obtain SDKs separately under Adobe's applicable agreements. No Adobe SDK headers, samples or development executables are included in the public repository. The plug-in is independent and is not certified or endorsed by Adobe.

## Little CMS

The Photoshop filter statically links [Little CMS 2.19.1](https://github.com/mm2/Little-CMS/releases/tag/lcms2.19.1) for ICC color conversion. Copyright (c) 1998–2025 Marti Maria Saguer; MIT, with the full notice in `licenses/LCMS-MIT.txt` and the binary package. Source archive SHA-256: `06D59795CB87A67F73DAAA5B6F86BF77B256710AAD5822E3FEE7F89546A5F93F`.

## Comparison images

The gallery uses the publisher's supplied game-footage comparison images, with before/after labels confirmed by the publisher and visually checked for matching framing. These images are demonstration material, not original project artwork covered by the source MIT license. The source material's owners retain their rights. See `docs/gallery/PROVENANCE.md`.

## Installer tooling

Windows setup is built with NSIS 3.12 from the official NSIS distribution. NSIS compiler/core is available under the zlib/libpng license; its components retain their applicable notices. See licenses/NSIS.txt. The compiler distribution is a build dependency, not a release asset. Branding assets in this project are original vector/code-generated artwork.

## Names and scope

NVIDIA and DLSS are NVIDIA marks. Adobe, Photoshop, After Effects and Premiere Pro are Adobe marks. Mention of these products identifies compatibility and does not imply affiliation, sponsorship or endorsement. This application does not implement NVIDIA's complete 3D-guided neural-rendering pipeline.
