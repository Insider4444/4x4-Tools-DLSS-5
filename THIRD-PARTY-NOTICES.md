# Third-party notices

## 4x4-Tools plug-in and installer source

Copyright (c) 2026 4x4-Tools and contributors. MIT; see LICENSE. This license covers original project code, not third-party SDKs or model binaries.

## Neural backend

Adapted from SAOG0721/DaVinci-Resolve-DLSS5, commit `a659e5c674388ea8026f4cf8df9f206826d12452`, MIT. Copyright notice and license are in licenses/UPSTREAM-MIT.txt and src/neural/UPSTREAM-LICENSE.txt. Adobe adaptations and provenance are recorded in src/neural/PROVENANCE.txt.

## NVIDIA components

**This software contains source code provided by NVIDIA Corporation.** The compiled plug-in links the NVIDIA NGX SDK. The build used NVIDIA/DLSS commit `a291cc7d2cc642a51566f3dfd5376f635cd1b284`. The SDK's license is included in licenses/NVIDIA-RTX-SDK.txt and each binary package. SDK source, headers and standalone libraries are not distributed in this repository.

Release packages contain a separately licensed, community-modified `runtime/nvngx_dlssnr.dll`, obtained with the Resolve DLSS5 Experimental v0.3.1 package. Its file version is 310.8.0.0 and SHA-256 is:

`984BEE0F775C277D5829B8FD6775D53A7B0F75396C852B3AAF06A18375F81014`

This is not represented as an untouched official NVIDIA release. The publisher has confirmed permission to redistribute the supplied binary with this application. That confirmation does not relicense the binary as MIT, grant SDK source redistribution rights, or establish support on every GPU. NVIDIA components retain their applicable terms and ownership. Downstream distributors must obtain and satisfy the rights and obligations applicable to their distribution; this repository grants no additional NVIDIA rights.

## Adobe SDK

Adobe's SDK supplies build-time interfaces and PiPL tooling. Obtain it separately under Adobe's applicable agreement. No Adobe SDK headers, samples or development executables are included in the public repository. The plug-in is independent and is not certified or endorsed by Adobe.

## Installer tooling

Windows setup is built with NSIS 3.12 from the official NSIS distribution. NSIS compiler/core is available under the zlib/libpng license; its components retain their applicable notices. See licenses/NSIS.txt. The compiler distribution is a build dependency, not a release asset. Branding assets in this project are original vector/code-generated artwork.

## Names and scope

NVIDIA and DLSS are NVIDIA marks. Adobe, After Effects and Premiere Pro are Adobe marks. Mention of these products identifies compatibility and does not imply affiliation, sponsorship or endorsement. This application does not implement NVIDIA's complete 3D-guided neural-rendering pipeline.
