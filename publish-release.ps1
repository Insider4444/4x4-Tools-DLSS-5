#Requires -Version 5.1
[CmdletBinding()]
param([string]$Version,[string]$NotesFile,[switch]$Publish)
$ErrorActionPreference='Stop'
if ($Publish) { & (Join-Path $PSScriptRoot 'deploy-main.ps1') -Version $Version -NotesFile $NotesFile; return }
. (Join-Path $PSScriptRoot 'scripts\common.ps1')
[void](Read-ReleaseConfig $Version)
& (Join-Path $PSScriptRoot 'scripts\build.ps1')
& (Join-Path $PSScriptRoot 'scripts\package.ps1')
$package=Get-Content (Join-Path $PSScriptRoot 'build\package-result.json') -Raw | ConvertFrom-Json
& (Join-Path $PSScriptRoot 'tests\setup_exe_test.ps1') -SetupExe $package.exe -Zip $package.zip
Write-Host ('Local candidate verified: '+$package.dist+'. Run deploy-main.ps1 after Adobe testing to have GitHub build and publish the final release.')
