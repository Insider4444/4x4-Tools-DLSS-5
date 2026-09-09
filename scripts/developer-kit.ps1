#Requires -Version 5.1
[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'common.ps1')
$config=Read-ReleaseConfig; $settings=Read-BuildSettings
$stamp=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,8)
$output=Join-Path $ProjectRoot ('artifacts\private-kit-'+$stamp)
$kit=Join-Path $output '4x4Tools-DLSS5-developer'
New-Item -ItemType Directory -Path $kit -Force | Out-Null
foreach ($file in @(Get-PublicFiles)) {
    $destination=Join-Path $kit $file
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination)) | Out-Null
    Copy-Item -LiteralPath (Join-Path $ProjectRoot $file) -Destination $destination
}
# Bundle public Git objects and refs, never .git/config, credentials or a CLI account.
[void](Git @('bundle','create',(Join-Path $kit 'repository.bundle'),'main','--tags'))
foreach ($pair in @(@{Source=$settings.AdobeSdk;Name='ae-sdk'},@{Source=$settings.NgxSdk;Name='nvidia-dlss'},@{Source=(Split-Path -Parent $settings.MakeNsis);Name='nsis'})) {
    $source=[IO.Path]::GetFullPath($pair.Source).TrimEnd('\')
    if ((Get-Item -LiteralPath $source -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Linked dependency roots cannot be archived.' }
    foreach ($item in @(Get-ChildItem -LiteralPath $source -Recurse -Force)) {
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw ('Linked dependency cannot be archived: '+$item.FullName) }
        $relative=$item.FullName.Substring($source.Length+1)
        if ($relative -match '(^|[\\/])(\.git|\.local|\.env[^\\/]*)([\\/]|$)') { continue }
        if (-not $item.PSIsContainer) {
            $destination=Join-Path $kit ('dependencies\'+$pair.Name+'\'+$relative)
            [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination)) | Out-Null
            Copy-Item -LiteralPath $item.FullName -Destination $destination
        }
    }
}
$runtime=Join-Path $kit 'dependencies\runtime\nvngx_dlssnr.dll'
[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($runtime)) | Out-Null
Copy-Item -LiteralPath $settings.Runtime -Destination $runtime
Write-JsonFile @{AdobeSdk='dependencies/ae-sdk';NgxSdk='dependencies/nvidia-dlss';Runtime='dependencies/runtime/nvngx_dlssnr.dll';MakeNsis='dependencies/nsis/makensis.exe'} (Join-Path $kit '.local\build-settings.json')
$instructions=@'
PRIVATE 4x4Tools-DLSS5 developer kit - do not upload this archive to GitHub.

Includes source and dotfiles, public Git history, local Adobe/NVIDIA SDKs,
the authorized neural runtime and portable NSIS with its license files.
SDK/runtime terms remain separate from the source MIT license.
No GitHub token, account, machine Git config, footage or project is included.

On another Windows x64 PC:
1. Extract to a short writable path, such as C:\Dev (not Program Files or
   Adobe MediaCore). Deeply nested paths can exceed MSVC's path limit.
2. Install Git and Visual Studio 2022/2026 Build Tools: Desktop development
   with C++, CMake tools and a Windows SDK. Install GitHub CLI for publishing.
3. Open PowerShell in this folder and run .\setup-dev.ps1.
   It restores Git history and resolves the bundled dependencies relative to
   this folder. Do not use git init/add manually before setup-dev.
4. Run gh auth login with YOUR account, then git pull --ff-only origin main
   if you need newer public code. Integrate any local changes normally.
5. Change code, save and close Adobe, run .\debug-install.ps1, and test footage.
6. Set version in release-config.json and edit update-note.md (Markdown).
   .\publish-release.ps1 prepares and tests packages without uploading.
   .\deploy-main.ps1 commits, pushes and triggers GitHub Actions to BUILD and
   publish the new version. -BuildOnly triggers a build without a public release.
7. Run .\scripts\developer-kit.ps1 to refresh this private portable archive.

Public Releases contain only the installer EXE, manual-install ZIP and hashes.
The ignored dependencies/, .local/, repository.bundle and private kit must
never be force-added to Git. See docs/DEVELOPING.md for recovery and details.
The Microsoft compiler, Adobe apps and NVIDIA driver are installed separately.
A compatible RTX GPU is required for full build tests and Adobe processing.
'@
[IO.File]::WriteAllText((Join-Path $kit 'PRIVATE-DEVELOPER-KIT.txt'),$instructions,[Text.UTF8Encoding]::new($false))
$files=@(Get-ChildItem -LiteralPath $kit -Recurse -Force -File)
$records=@($files | ForEach-Object { @{path=$_.FullName.Substring($kit.Length+1).Replace('\','/');sha256=(Get-Sha $_.FullName)} })
Write-JsonFile @{version=$config.version;created=(Get-Date -Format o);files=$records} (Join-Path $kit '.local\kit-manifest.json')
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zipFile=Join-Path $output ('4x4Tools-DLSS5-win-v'+$config.version+'-PRIVATE-developer-kit.zip')
$zip=[IO.Compression.ZipFile]::Open($zipFile,[IO.Compression.ZipArchiveMode]::Create)
try {
    foreach ($file in @(Get-ChildItem -LiteralPath $kit -Recurse -Force -File)) {
        $relative=$file.FullName.Substring($kit.Length+1).Replace('\','/')
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip,$file.FullName,('4x4Tools-DLSS5-developer/'+$relative),[IO.Compression.CompressionLevel]::Optimal) | Out-Null
    }
} finally { $zip.Dispose() }
[IO.File]::WriteAllText((Join-Path $output 'SHA256SUMS.txt'),(Get-Sha $zipFile)+'  '+[IO.Path]::GetFileName($zipFile)+"`n",[Text.Encoding]::ASCII)
Write-JsonFile @{zip=$zipFile;folder=$kit;version=$config.version} (Join-Path $ProjectRoot '.local\developer-kit-result.json')
Write-Host ('Private portable developer kit: '+$zipFile)
