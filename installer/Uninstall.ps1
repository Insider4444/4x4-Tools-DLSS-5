#Requires -Version 5.1
$ErrorActionPreference='Stop'
$shell=Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
if (-not [Environment]::Is64BitProcess) { $shell=Join-Path $env:SystemRoot 'Sysnative\WindowsPowerShell\v1.0\powershell.exe' }
$engine=Join-Path $PSScriptRoot 'installer-engine.ps1'
$arguments='-NoLogo -NoProfile -ExecutionPolicy Bypass -File "'+$engine+'" -Action Uninstall'
try {
    $process=Start-Process -FilePath $shell -ArgumentList $arguments -Verb RunAs -Wait -PassThru -WindowStyle Hidden
    if ($process.ExitCode -ne 0) { throw 'Uninstall failed. Read the latest log in ProgramData\4x4-Tools\DLSS-5\Logs.' }
    Write-Host '4x4-Tools uninstalled. Backups and logs were preserved.'
} catch { Write-Error $_; exit 1 }
