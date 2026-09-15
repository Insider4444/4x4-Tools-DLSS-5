#Requires -Version 5.1
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$SetupExe,[Parameter(Mandatory=$true)][string]$Zip)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$testRoot=Join-Path $repo ('artifacts\compiled-setup-'+[Guid]::NewGuid().ToString('N'))
$sandbox=Join-Path $testRoot 'test install with spaces'
$expanded=Join-Path $testRoot 'zip'
New-Item -ItemType Directory -Force -Path $sandbox,$expanded | Out-Null
Set-Content -LiteralPath (Join-Path $sandbox '.4x4-installer-test-root') -Value 'Isolated compiled installer test.'
$target=Join-Path $sandbox 'MediaCore\4x4Tools-DLSS5'
$photoshopTarget=Join-Path $sandbox 'Photoshop\4x4Tools-DLSS5'
$maintenance=Join-Path $sandbox 'Maintenance'
function Assert([bool]$Condition,[string]$Message) { if (-not $Condition) { throw $Message } }
function Run-Exe([string]$Exe,[string]$Arguments) {
    $start=New-Object Diagnostics.ProcessStartInfo
    $start.FileName=[IO.Path]::GetFullPath($Exe); $start.Arguments=$Arguments
    $start.UseShellExecute=$false; $start.CreateNoWindow=$true
    $taskPath=[Environment]::GetEnvironmentVariable('Path','Process')
    foreach ($key in @($start.EnvironmentVariables.Keys)) { if ([string]$key -ieq 'Path') { $start.EnvironmentVariables.Remove([string]$key) } }
    $start.EnvironmentVariables['Path']=$taskPath
    # The marked test root redirects every installation write and disables registry changes.
    # Keep this owned test process at the caller's privilege level: no elevation is needed.
    $start.EnvironmentVariables['__COMPAT_LAYER']='RunAsInvoker'
    $process=New-Object Diagnostics.Process; $process.StartInfo=$start
    try {
        [void]$process.Start()
        if (-not $process.WaitForExit(120000)) { $process.Kill(); throw 'Compiled setup test timed out.' }
        Assert ($process.ExitCode -eq 0) ("Setup returned $($process.ExitCode). Inspect logs below $sandbox")
    } finally { $process.Dispose() }
}
$testArgument='/TESTROOT="'+$sandbox+'"'
Run-Exe $SetupExe ('/S /VALIDATEONLY '+$testArgument)
Assert (-not (Test-Path -LiteralPath $target)) 'Validate-only installed the plug-in.'
Assert (-not (Test-Path -LiteralPath $photoshopTarget)) 'Validate-only installed the Photoshop plug-in.'
$logs=Join-Path $sandbox 'State\Logs'
Assert (@(Get-ChildItem -LiteralPath $logs -Filter 'Validate-*.log').Count -gt 0) 'No successful validation log was produced.'
Write-Host 'PASS compiled EXE extraction and real GPU validation'
Run-Exe $SetupExe ('/S '+$testArgument)
Assert (Test-Path -LiteralPath (Join-Path $target '4x4Tools-DLSS5.aex')) 'Compiled EXE did not install.'
Assert (Test-Path -LiteralPath (Join-Path $maintenance 'Uninstall.exe')) 'Uninstaller was not installed.'
Write-Host 'PASS compiled EXE installs to a marked directory containing spaces'
Expand-Archive -LiteralPath $Zip -DestinationPath $expanded
$manifest=Get-Content -LiteralPath (Join-Path $expanded '4x4Tools-DLSS5\install-manifest.json') -Raw | ConvertFrom-Json
Assert (Test-Path -LiteralPath (Join-Path $expanded 'README.txt')) 'Manual installation README is missing.'
Assert (@(Get-ChildItem -LiteralPath $expanded -Recurse -File | Where-Object { $_.Extension -match '^\.(ps1|cpp|h|lib|pdb)$' }).Count -eq 0) 'Development files leaked into the public ZIP.'
foreach ($file in $manifest.files) {
    $fromZip=(Get-FileHash -LiteralPath (Join-Path (Join-Path $expanded '4x4Tools-DLSS5') $file.path)).Hash
    $fromExe=(Get-FileHash -LiteralPath (Join-Path $target $file.path)).Hash
    Assert ($fromZip -eq $file.sha256 -and $fromExe -eq $fromZip) ('EXE/ZIP payload mismatch: '+$file.path)
    $fromPhotoshop=(Get-FileHash -LiteralPath (Join-Path $photoshopTarget $file.path)).Hash
    Assert ($fromPhotoshop -eq $fromZip) ('Photoshop destination mismatch: '+$file.path)
}
# Git checkout enforces CRLF for PS1 files; editor working files can use LF.
# Compare exact script text after normalizing that checkout-only difference.
$sourceEngine=[IO.File]::ReadAllText((Join-Path $repo 'installer\installer-engine.ps1')).Replace("`r`n","`n")
$compiledEngine=[IO.File]::ReadAllText((Join-Path $maintenance 'installer-engine.ps1')).Replace("`r`n","`n")
Assert ($sourceEngine -ceq $compiledEngine) 'Compiled installer engine differs from source.'
Write-Host 'PASS every EXE and ZIP payload file has the same reviewed hash'
Run-Exe (Join-Path $maintenance 'Uninstall.exe') ('/S '+$testArgument)
# NSIS relocates its uninstaller to a temporary process so it can remove itself.
$timer=[Diagnostics.Stopwatch]::StartNew()
while ((Test-Path -LiteralPath (Join-Path $maintenance 'Uninstall.exe')) -and $timer.Elapsed.TotalSeconds -lt 60) { Start-Sleep -Milliseconds 250 }
Assert (-not (Test-Path -LiteralPath (Join-Path $target '4x4Tools-DLSS5.aex'))) 'Uninstaller did not remove the owned plug-in.'
Assert (-not (Test-Path -LiteralPath (Join-Path $photoshopTarget '4x4Tools-DLSS5-Photoshop.8bf'))) 'Uninstaller did not remove the Photoshop plug-in.'
Assert (-not (Test-Path -LiteralPath $maintenance)) 'Uninstaller did not remove its maintenance files.'
Assert (@(Get-ChildItem -LiteralPath $logs -Filter 'Uninstall-*.log').Count -gt 0) 'No uninstall log was produced.'
Write-Host 'PASS compiled uninstaller removes the payload and itself, preserving logs'
@{status='passed';tests=@('compiled-validation','compiled-install','identical-exe-zip-payload','compiled-uninstall');created=(Get-Date -Format o)} |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $testRoot 'result.json') -Encoding UTF8
Write-Host ('Compiled setup checks passed: '+$testRoot)
