#Requires -Version 5.1
[CmdletBinding()]
param([switch]$BuildOnly)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'scripts\common.ps1')
if (-not $BuildOnly) { Assert-AdobeClosed }
[void](Sync-ReleaseVersion); [void](Read-BuildSettings)
$fingerprint=Get-SourceFingerprint -CodeOnly
$receiptPath=Join-Path $PSScriptRoot '.local\local-install-receipt.json'
if (Test-Path -LiteralPath $receiptPath) {
    $previous=Get-Content -LiteralPath $receiptPath -Raw | ConvertFrom-Json
    if ($previous.coreFingerprint -eq $fingerprint) { Write-Host 'Code is unchanged since the last local install. Verifying the incremental build.' }
    else { Write-Host 'Code changes detected. Building the updated plug-in.' }
}
& (Join-Path $PSScriptRoot 'scripts\build.ps1')
if ($BuildOnly) { Write-Host 'Build verified. Adobe installation was not requested.'; return }
& (Join-Path $PSScriptRoot 'scripts\package.ps1') -PayloadOnly
$package=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'build\package-result.json') -Raw | ConvertFrom-Json
if ((Get-SourceFingerprint -CodeOnly) -ne $fingerprint) { throw 'Code changed during the build. Run debug-install.ps1 again.' }
Assert-AdobeClosed
$shell=Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
$arguments=@('-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $PSScriptRoot 'installer\installer-engine.ps1'),'-Action','Install','-PackageDir',(Join-Path $package.packageDir 'payload'))
$identity=[Security.Principal.WindowsIdentity]::GetCurrent()
$admin=(New-Object Security.Principal.WindowsPrincipal($identity)).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if ($admin) { [void](Invoke-Native $shell $arguments) }
else {
    $process=Start-Process -FilePath $shell -ArgumentList (($arguments | ForEach-Object { Quote-Native $_ }) -join ' ') -Verb RunAs -WindowStyle Hidden -Wait -PassThru
    if ($process.ExitCode -ne 0) { throw 'Installation did not complete. See ProgramData\4x4-Tools\DLSS-5\Logs. The installer preserves the previous version on failure.' }
}
$target=Join-Path ([Environment]::GetFolderPath('ProgramFiles')) 'Adobe\Common\Plug-ins\7.0\MediaCore\4x4Tools-DLSS5'
$manifest=Get-Content -LiteralPath (Join-Path $package.packageDir 'payload\install-manifest.json') -Raw | ConvertFrom-Json
foreach ($file in $manifest.files) { if ((Get-Sha (Join-Path $target $file.path)) -ne $file.sha256) { throw ('Installed file verification failed: '+$file.path) } }
Write-JsonFile @{version=$package.version;coreFingerprint=$fingerprint;moduleSHA=(Get-Sha (Join-Path $target '4x4Tools-DLSS5.aex'));installedAt=(Get-Date -Format o)} $receiptPath
Write-Host ('Installed and verified v'+$package.version+'. Open AE/Premiere and test your footage. When satisfied, edit update-note.md and run deploy-main.ps1 to build and publish through GitHub Actions.')
