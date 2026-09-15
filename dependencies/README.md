# Private build dependencies

Configure paths with setup-dev.ps1. The full suite needs:

- Adobe After Effects SDK interfaces, utilities and PiPL tool.
- Windows Photoshop SDK 2026 v2, using its pluginsdk folder.
- NVIDIA DLSS SDK headers and static NGX library.
- Little CMS 2.19.1 sources, headers and MIT license.
- The neural runtime pinned in resources/runtime.json.
- NSIS 3.12 portable compiler.

Dependencies stay in this ignored folder, external local paths or the private
build-dependency repository. Public Git and release ZIPs contain no SDK headers,
libraries or development tools. The private developer kit can carry the owner's
authorized copies. See docs/BUILDING.md and THIRD-PARTY-NOTICES.md.