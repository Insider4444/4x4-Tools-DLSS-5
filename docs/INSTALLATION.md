# Installation and troubleshooting

## EXE (recommended)

Download the EXE from Releases, save projects and fully exit AE and Premiere. Run setup and approve Windows elevation. Setup checks all payload hashes, verifies an Adobe installation and tests neural rendering on the actual GPU. It never force-closes an Adobe process. Follow the progress details if a check fails.

The current release is unsigned. Check its source and published SHA-256 before running an unfamiliar download. A checksum checks the downloaded bytes against the published release; it is not an Authenticode signature.

The shared plug-in is installed in:

`C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\4x4Tools-DLSS5`

Maintenance files and the uninstaller are in `C:\Program Files\4x4-Tools\DLSS-5`. Setup creates **4x4-Tools DLSS 5** in Windows Installed apps. Reopen Adobe and search for **4x4Tools-DLSS5**. In AE it is under **Effect → 4x4Tools**; in Premiere search the Effects panel.

## ZIP (manual installation)

Extract the ZIP and read README.txt. It contains one `4x4Tools-DLSS5` folder with the compiled effect, required runtime/helpers, licenses and controls guide. It has no source, SDKs or PowerShell build/install scripts.

1. Save and close Adobe. Run `4x4Tools-DLSS5\SupportCheck.exe` in a terminal and continue only if it reports `ok: true`.
2. Back up the previous exact plug-in folder outside Adobe's scan paths.
3. Copy the complete `4x4Tools-DLSS5` folder to `C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore`, accepting Windows elevation.
4. Reopen Adobe and search for the effect. Keep the `runtime` subfolder intact; do not copy only the `.aex`.

The EXE performs integrity checks and transaction rollback automatically. Manual copying does not create a Windows uninstall entry; remove only the exact plug-in folder after closing Adobe to undo a manual installation. Keep backups outside MediaCore to avoid duplicate effects.

## Updates and removal

Existing files in the exact 4x4Tools-DLSS5 folder are backed up to `C:\ProgramData\4x4-Tools\DLSS-5\Backups` before replacement. Backups are outside Adobe's plug-in search path. The new files are staged and verified; a commit failure attempts to restore the prior installation. Backups and logs remain after removal.

Remove EXE installations through Windows **Installed apps → 4x4-Tools DLSS 5 → Uninstall**. Close Adobe first. Uninstall removes only recognized files that still match the installation manifest; unknown or modified files are preserved and listed in the log. Such a retained modified `.aex` can still be discovered by Adobe. Review it manually if complete removal is intended. Other Adobe plug-ins are not removed.

To restore a backup manually, close Adobe, move the current exact 4x4Tools folder outside MediaCore and restore the chosen backup's `plugin` folder under the exact original name. Keep the current folder until the restored version is verified. Do not restore an entire MediaCore directory over unrelated plug-ins.

## Error messages

| Symptom | Next step |
| --- | --- |
| Adobe is still running | Save and fully exit both apps and their render processes. Setup does not kill them. |
| File missing / integrity failure | Download a fresh EXE or ZIP. Extract all ZIP contents together. Check quarantine history if a required file disappeared. |
| No compatible NVIDIA RTX GPU | The included runtime cannot run on the detected device. See the compatibility matrix. |
| Neural test failed / stopped unexpectedly | Update the NVIDIA driver, restart Windows and retry with other GPU workloads closed. If it still fails, include the log in an issue. A different supported runtime may be needed; v1.0 does not fetch one automatically. |
| GPU test timed out | The child checker is stopped after 60 seconds. No plug-in replacement occurs. Restart and retry; report repeated timeouts. |
| Adobe is not installed | Install AE or Premiere first. Standard Program Files locations and installed-application registry entries are checked. |
| Effect not visible | Restart Adobe after setup; confirm the exact installed folder, keep its runtime subfolder, and remove older duplicate copies from scan paths after backing them up. Search by the full effect name. |
| Project is slow | Preview at a lower resolution, reduce concurrent GPU workloads and try neutral restoration controls. Additional restoration has a CPU cost. Compare with the effect bypassed. |
| Visible artifacts or flicker | Reduce neural intensity/tone, increase color/lighting/texture preservation, and inspect the wipe. Recipes do not guarantee realism for every shot. |

Setup logs: `C:\ProgramData\4x4-Tools\DLSS-5\Logs`. Validation-only and early setup failures: `%TEMP%\4x4-Tools-DLSS-5-Validation\Logs`. GPU-check details are stored with those logs. Plug-in diagnostics during Adobe use: `%LOCALAPPDATA%\AdobeDlss5\AdobeDlss5.log`.

Logs are local. No support data is uploaded automatically. Before sharing, review paths and remove names or project details you consider private.

## Subtle update notice (v1.1 and later)

The small version row at the bottom of the effect controls changes to **Update X available** when a newer stable GitHub release is cached. Use **Updates** to enable or disable automatic checks, check now, or open the release page. Reopen or interact with the effect controls after a check to refresh the row. There are no dialogs or automatic installations.

Checks use a separate helper process, at most once a day, started from effect UI callbacks. Rendering and Adobe start-up do not perform network requests. GitHub receives a normal HTTPS request (including the IP address and plug-in version in the user agent); no footage, project names, GPU details or account credentials are sent. Offline errors stay silent. Settings/cache are in %LOCALAPPDATA%\4x4Tools\Updates. Set the TOOLS_DISABLE_UPDATE_CHECK environment variable before starting Adobe to suppress all helper launches.

Existing v1.0 binaries need an upgrade once to gain this feature. The current stable release remains v1.0 until a newer version is published.
