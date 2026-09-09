# 4x4Tools-DLSS5-win 1.1.0

## Added

- Portable developer kit with local SDKs, runtime, installer compiler and Git history restore.
- `debug-install.ps1` builds, tests and installs local code for Adobe testing.
- `deploy-main.ps1` detects changes and triggers GitHub Actions to build and publish the exact committed version, using these Markdown notes. Local `publish-release.ps1` remains available for installer testing.
- Subtle in-effect update status with daily background checks, an off switch, manual checking and a release-page action.

## Notes

- Edit these notes before publishing. Set the next version in `release-config.json`.
- Existing v1.0 installations need this new plug-in installed once to gain the update notice.
- Neural processing and GPU compatibility limits remain as documented.
