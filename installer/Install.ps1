#Requires -Version 5.1
[CmdletBinding()]
param([switch]$ValidateOnly)
$ErrorActionPreference='Stop'
$engine=Join-Path $PSScriptRoot 'installer-engine.ps1'
$payload=Join-Path $PSScriptRoot 'payload'
$action=if ($ValidateOnly) {'Validate'} else {'Install'}
$shell=Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
if (-not [Environment]::Is64BitProcess) { $shell=Join-Path $env:SystemRoot 'Sysnative\WindowsPowerShell\v1.0\powershell.exe' }
# File names cannot contain quotes; arguments go to PowerShell's -File parser, not -Command.
$arguments='-NoLogo -NoProfile -ExecutionPolicy Bypass -File "'+$engine+'" -Action '+$action+' -PackageDir "'+$payload+'"'
try {
    $parameters=@{FilePath=$shell;ArgumentList=$arguments;Wait=$true;PassThru=$true;WindowStyle='Hidden'}
    if (-not $ValidateOnly) { $parameters.Verb='RunAs' }
    $process=Start-Process @parameters
    if ($process.ExitCode -ne 0) { throw 'Setup failed. Read the latest log in ProgramData\4x4-Tools\DLSS-5\Logs (ValidateOnly: your temporary folder\4x4-Tools-DLSS-5-Validation\Logs).' }
    Write-Host '4x4-Tools completed successfully. Reopen Adobe and search Effects for 4x4Tools-DLSS5.'
} catch { Write-Error $_; exit 1 }
