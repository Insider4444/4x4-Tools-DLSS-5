#Requires -Version 5.1
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$PackageDir,[switch]$IncludeTimeout)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$testRoot=Join-Path $repo ('artifacts\installer-'+[Guid]::NewGuid().ToString('N'))
$sandbox=Join-Path $testRoot 'test install'
$payload=Join-Path $testRoot 'payload'
New-Item -ItemType Directory -Force -Path $sandbox,$payload | Out-Null
Set-Content -LiteralPath (Join-Path $sandbox '.4x4-installer-test-root') -Value 'Isolated installer integration test.'
Copy-Item -Path (Join-Path $PackageDir 'payload\*') -Destination $payload -Recurse
$engine=Join-Path $repo 'installer\installer-engine.ps1'
$shell=Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
$target=Join-Path $sandbox 'MediaCore\4x4Tools-DLSS5'
$photoshopTarget=Join-Path $sandbox 'Photoshop\4x4Tools-DLSS5'
$maintenance=Join-Path $sandbox 'Maintenance'
$results=New-Object Collections.Generic.List[string]
function Assert([bool]$Condition,[string]$Message) { if (-not $Condition) { throw $Message } }
function Run([string]$Action,[int]$Expected,[string]$Name) {
    $start=New-Object Diagnostics.ProcessStartInfo
    $start.FileName=$shell; $start.UseShellExecute=$false; $start.CreateNoWindow=$true
    $start.RedirectStandardOutput=$true; $start.RedirectStandardError=$true
    $start.Arguments='-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "'+$engine+'" -Action '+$Action+' -PackageDir "'+$payload+'" -SandboxRoot "'+$sandbox+'"'
    $taskPath=[Environment]::GetEnvironmentVariable('Path','Process')
    foreach ($key in @($start.EnvironmentVariables.Keys)) { if ([string]$key -ieq 'Path') { $start.EnvironmentVariables.Remove([string]$key) } }
    $start.EnvironmentVariables['Path']=$taskPath
    $process=New-Object Diagnostics.Process; $process.StartInfo=$start
    try {
        [void]$process.Start()
        $stdout=$process.StandardOutput.ReadToEndAsync(); $stderr=$process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit(90000)) { $process.Kill(); throw "Test hung: $Name" }
        $result=$stdout.Result+$stderr.Result
        $result | Set-Content -LiteralPath (Join-Path $testRoot ($Name+'.log')) -Encoding UTF8
        Assert ($process.ExitCode -eq $Expected) ("$Name returned $($process.ExitCode), expected $Expected : $result")
        $results.Add($Name); Write-Host "PASS $Name"
        return $result
    } finally { $process.Dispose() }
}
$manifestPath=Join-Path $payload 'install-manifest.json'
$originalManifest=Get-Content -LiteralPath $manifestPath -Raw
$originalChecker=Join-Path $PackageDir 'payload\SupportCheck.exe'
function Restore-Package {
    $originalManifest | Set-Content -LiteralPath $manifestPath -Encoding UTF8
    Copy-Item -LiteralPath $originalChecker -Destination (Join-Path $payload 'SupportCheck.exe') -Force
}
function Stub-Checker([string]$Body) {
    $source=Join-Path $testRoot 'checker.cs'
    ('using System; using System.Threading; class Check { static int Main() { '+$Body+' } }') | Set-Content -LiteralPath $source -Encoding ASCII
    $compiler=Join-Path $env:SystemRoot 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
    & $compiler '/nologo' '/target:exe' ('/out:'+(Join-Path $payload 'SupportCheck.exe')) $source
    if ($LASTEXITCODE -ne 0) { throw 'Test checker could not be compiled.' }
    $manifest=$originalManifest | ConvertFrom-Json
    foreach ($file in $manifest.files) { if ($file.path -eq 'SupportCheck.exe') { $file.sha256=(Get-FileHash -LiteralPath (Join-Path $payload 'SupportCheck.exe')).Hash } }
    $manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
}
$mutex=New-Object Threading.Mutex($false,'Local\4x4-Tools-DLSS-5-Installer-Test')
[void]$mutex.WaitOne(0)
try {
    $result=Run 'Validate' 1 'concurrent-setup-rejected'
    Assert ($result -match 'Another 4x4-Tools setup') 'Concurrent setup was not diagnosed.'
} finally { $mutex.ReleaseMutex(); $mutex.Dispose() }
[void](Run 'Validate' 0 'validate-real-gpu')
Assert (-not (Test-Path -LiteralPath $target)) 'Validation changed the installation.'
[void](Run 'Install' 0 'fresh-install')
$hash=(Get-FileHash -LiteralPath (Join-Path $target '4x4Tools-DLSS5.aex')).Hash
Assert ($hash -eq (Get-FileHash -LiteralPath (Join-Path $payload '4x4Tools-DLSS5.aex')).Hash) 'Installed module differs.'
$psHash=(Get-FileHash -LiteralPath (Join-Path $photoshopTarget '4x4Tools-DLSS5-Photoshop.8bf')).Hash
Assert ($psHash -eq (Get-FileHash -LiteralPath (Join-Path $payload '4x4Tools-DLSS5-Photoshop.8bf')).Hash) 'Photoshop module differs.'
Set-Content -LiteralPath (Join-Path $target 'retained-note.txt') -Value 'Preserve user additions during upgrade/rollback.'

# Deliberately deny rename of the old maintenance directory after plug-in staging.
# This exercises the actual transaction rollback, without altering a live install.
$lock=[IO.File]::Open((Join-Path $maintenance 'installer-engine.ps1'),[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::None)
try {
    $result=Run 'Install' 1 'failed-commit-rolls-back'
    Assert ($result -match 'restoring the previous installation') 'Rollback was not reached.'
    Assert (Test-Path -LiteralPath (Join-Path $target 'retained-note.txt')) 'Rollback lost an original file.'
    Assert ((Get-FileHash -LiteralPath (Join-Path $target '4x4Tools-DLSS5.aex')).Hash -eq $hash) 'Rollback changed the prior module.'
    Assert ((Get-FileHash -LiteralPath (Join-Path $photoshopTarget '4x4Tools-DLSS5-Photoshop.8bf')).Hash -eq $psHash) 'Rollback changed Photoshop.'
} finally { $lock.Dispose() }
[void](Run 'Install' 0 'upgrade-with-backup')
$backups=@(Get-ChildItem -LiteralPath (Join-Path $sandbox 'State\Backups') -Filter 'retained-note.txt' -Recurse)
Assert ($backups.Count -gt 0) 'Upgrade did not preserve the original folder.'

Add-Content -LiteralPath (Join-Path $payload 'CONTROLS.md') -Value 'tamper'
$result=Run 'Install' 1 'tampered-payload-rejected'
Assert ($result -match 'integrity check failed') 'Tampering was not diagnosed.'
Copy-Item -LiteralPath (Join-Path $PackageDir 'payload\CONTROLS.md') -Destination (Join-Path $payload 'CONTROLS.md') -Force
$manifest=$originalManifest | ConvertFrom-Json; $manifest.files[0].path='../outside.dll'
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
$result=Run 'Install' 1 'manifest-traversal-rejected'
Assert ($result -match 'invalid or duplicate') 'Manifest traversal was not diagnosed.'
Restore-Package

Stub-Checker 'Console.WriteLine("{\"ok\":false,\"message\":\"Unsupported test GPU\"}"); return 11;'
$result=Run 'Install' 1 'unsupported-gpu-preserves-install'
Assert ($result -match 'Unsupported test GPU') 'GPU error was not preserved.'
Assert ((Get-FileHash -LiteralPath (Join-Path $target '4x4Tools-DLSS5.aex')).Hash -eq $hash) 'Failed GPU check replaced the installation.'
Restore-Package
if ($IncludeTimeout) {
    Stub-Checker 'Thread.Sleep(600000); return 0;'
    $result=Run 'Install' 1 'gpu-timeout-preserves-install'
    Assert ($result -match 'timed out after 60 seconds') 'Timeout was not diagnosed.'
    Assert ((Get-FileHash -LiteralPath (Join-Path $target '4x4Tools-DLSS5.aex')).Hash -eq $hash) 'Timeout replaced the installation.'
    Restore-Package
}
Add-Content -LiteralPath (Join-Path $target 'CONTROLS.md') -Value 'User edited documentation.'
Set-Content -LiteralPath (Join-Path $target 'user-file.txt') -Value 'Must survive uninstall.'
[void](Run 'Uninstall' 0 'uninstall-preserves-foreign-and-modified-files')
Assert (-not (Test-Path -LiteralPath (Join-Path $target '4x4Tools-DLSS5.aex'))) 'Uninstall left its unmodified module.'
Assert (-not (Test-Path -LiteralPath (Join-Path $photoshopTarget '4x4Tools-DLSS5-Photoshop.8bf'))) 'Uninstall left Photoshop installed.'
Assert (Test-Path -LiteralPath (Join-Path $target 'user-file.txt')) 'Uninstall deleted a foreign file.'
Assert (Test-Path -LiteralPath (Join-Path $target 'CONTROLS.md')) 'Uninstall deleted a modified file.'
@{status='passed';tests=@($results);root=$testRoot;created=(Get-Date -Format o)} | ConvertTo-Json -Depth 4 |
    Set-Content -LiteralPath (Join-Path $testRoot 'result.json') -Encoding UTF8
Write-Host ("All $($results.Count) installer scenarios passed. Results: $testRoot")
