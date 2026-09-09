#Requires -Version 5.1
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'common.ps1')
if ($env:GITHUB_ACTIONS -ne 'true') { throw 'Use scripts/build.ps1 for local GPU validation.' }
$downloads=Join-Path $ProjectRoot '.local\runtime-download'
New-Item -ItemType Directory -Path $downloads -Force | Out-Null
[void](Gh @('release','download','v1.0','--repo','Insider4444/4x4-Tools-DLSS-5','--pattern','4x4Tools-DLSS5-win-v1.0.zip','--dir',$downloads))
Expand-Archive -LiteralPath (Join-Path $downloads '4x4Tools-DLSS5-win-v1.0.zip') -DestinationPath (Join-Path $downloads 'expanded')
$runtime=Join-Path $downloads 'expanded\4x4Tools-DLSS5\runtime\nvngx_dlssnr.dll'
if ((Get-Sha $runtime) -ne '984bee0f775c277d5829b8fd6775d53a7b0f75396c852b3aaf06a18375f81014') { throw 'Downloaded runtime does not match the reviewed runtime.' }
$sdk=Join-Path $ProjectRoot '.local\ci-sdk'
& (Join-Path $ProjectRoot 'setup-dev.ps1') -AdobeSdk (Join-Path $sdk 'ae-sdk') -NgxSdk (Join-Path $sdk 'nvidia-dlss') -MakeNsis (Join-Path $sdk 'nsis\makensis.exe') -Runtime $runtime
& (Join-Path $PSScriptRoot 'build.ps1') -HostedCI
& (Join-Path $PSScriptRoot 'package.ps1')
& (Join-Path $ProjectRoot 'tests\package_layout_test.ps1')
$package=Get-Content (Join-Path $ProjectRoot 'build\package-result.json') -Raw | ConvertFrom-Json
[IO.File]::AppendAllText($env:GITHUB_OUTPUT,('release_dir='+$package.dist+"`n"))
[IO.File]::AppendAllText($env:GITHUB_STEP_SUMMARY,("Built v"+$package.version+" from "+$env:GITHUB_SHA+".`n`nCPU, Adobe host contract, update helper and package integrity checks passed. Hosted runners have no NVIDIA GPU: neural hardware tests run locally through debug-install.ps1 and the installer checks each user's actual GPU.`n`nArtifacts contain only the EXE, manual-install ZIP and SHA256SUMS.txt.`n"))
