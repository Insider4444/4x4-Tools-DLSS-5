#Requires -Version 5.1
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'common.ps1')
if ($env:GITHUB_ACTIONS -ne 'true') { throw 'Use scripts/build.ps1 for local GPU validation.' }
$downloads=Join-Path $ProjectRoot '.local\runtime-download'
New-Item -ItemType Directory -Path $downloads -Force | Out-Null
$pin=Get-Content -LiteralPath (Join-Path $ProjectRoot 'resources\runtime.json') -Raw | ConvertFrom-Json
if ($pin.archiveUrl -notmatch '^https://github\.com/RankFTW/rhi-repo/releases/download/[^/]+/[^/]+\.zip$' -or $pin.archiveEntry -ne 'nvngx_dlssnr.dll') { throw 'Review the new runtime source before building.' }
$archive=Join-Path $downloads 'runtime.zip'
Invoke-WebRequest -Uri $pin.archiveUrl -OutFile $archive
if ((Get-Sha $archive) -ne $pin.archiveSha256) { throw 'Downloaded runtime archive digest mismatch.' }
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip=[IO.Compression.ZipFile]::OpenRead($archive)
$runtime=Join-Path $downloads 'nvngx_dlssnr.dll'
try {
    $entry=$zip.GetEntry($pin.archiveEntry)
    if (-not $entry) { throw 'Pinned runtime entry is missing.' }
    [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$runtime,$false)
} finally { $zip.Dispose() }
if ((Get-Sha $runtime) -ne $pin.sha256) { throw 'Downloaded runtime does not match the reviewed runtime.' }
$sdk=Join-Path $ProjectRoot '.local\ci-sdk'
& (Join-Path $ProjectRoot 'setup-dev.ps1') -AdobeSdk (Join-Path $sdk 'ae-sdk') -PhotoshopSdk (Join-Path $sdk 'photoshop-sdk') -Lcms (Join-Path $sdk 'lcms') -NgxSdk (Join-Path $sdk 'nvidia-dlss') -MakeNsis (Join-Path $sdk 'nsis\makensis.exe') -Runtime $runtime
& (Join-Path $PSScriptRoot 'build.ps1') -HostedCI
& (Join-Path $PSScriptRoot 'package.ps1')
& (Join-Path $ProjectRoot 'tests\package_layout_test.ps1')
$package=Get-Content (Join-Path $ProjectRoot 'build\package-result.json') -Raw | ConvertFrom-Json
[IO.File]::AppendAllText($env:GITHUB_OUTPUT,('release_dir='+$package.dist+"`n"))
[IO.File]::AppendAllText($env:GITHUB_STEP_SUMMARY,("Built v"+$package.version+" from "+$env:GITHUB_SHA+".`n`nCPU, Adobe host contract, update helper and package integrity checks passed. Hosted runners have no NVIDIA GPU: neural hardware tests run locally through debug-install.ps1 and the installer checks each user's actual GPU.`n`nArtifacts contain only the EXE, manual-install ZIP and SHA256SUMS.txt.`n"))
