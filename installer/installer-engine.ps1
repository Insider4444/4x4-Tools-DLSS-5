#Requires -Version 5.1
[CmdletBinding()]
param(
    [ValidateSet('Validate','Install','Uninstall')][string]$Action = 'Validate',
    [string]$PackageDir = $PSScriptRoot,
    [string]$MaintenanceDir,
    [string]$SandboxRoot
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2
$productId='4x4-Tools-DLSS-5'
$registryKey='HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\4x4-Tools-DLSS-5'
$allowed=@('4x4Tools-DLSS5.aex','SupportCheck.exe','runtime/nvngx_dlssnr.dll',
    'LICENSE.txt','THIRD-PARTY-NOTICES.txt','NVIDIA-RTX-SDK.txt','UPSTREAM-MIT.txt','CONTROLS.md')
$session=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[Guid]::NewGuid().ToString('N')
$logPath=$null
$installMutex=$null
$hasMutex=$false

function Full([string]$Path) {
    if (-not [IO.Path]::IsPathRooted($Path) -or $Path.StartsWith('\\')) { throw 'A local absolute path is required.' }
    [IO.Path]::GetFullPath($Path).TrimEnd('\')
}
function SafePath([string]$Path) {
    $current=Full $Path
    while ($current) {
        if (Test-Path -LiteralPath $current) {
            if ((Get-Item -LiteralPath $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw "Linked folders or files are not supported: $current"
            }
        }
        $parent=Split-Path -Parent $current
        if ($parent -eq $current) { break }
        $current=$parent
    }
}
function Contained([string]$Path,[string]$Root) {
    $resolved=Full $Path; $base=Full $Root
    if (-not $resolved.StartsWith($base+'\',[StringComparison]::OrdinalIgnoreCase)) { throw "Path escapes its installation folder: $resolved" }
    SafePath $resolved
    $resolved
}
function Log([string]$Message) {
    Write-Host $Message
    if ($logPath) { Add-Content -LiteralPath $logPath -Value ((Get-Date -Format o)+' '+$Message) -Encoding UTF8 }
}
function Hash([string]$Path) {
    # Use the framework directly so setup does not depend on PowerShell module search paths.
    $stream=[IO.File]::OpenRead($Path)
    $algorithm=[Security.Cryptography.SHA256]::Create()
    try { return [BitConverter]::ToString($algorithm.ComputeHash($stream)).Replace('-','') }
    finally { $algorithm.Dispose(); $stream.Dispose() }
}
function Remove-OwnedTree([string]$Path,[string]$Root) {
    $safe=Contained $Path $Root
    if (Test-Path -LiteralPath $safe) {
        # No recursive operation crosses a junction, symlink or the approved root.
        Get-ChildItem -LiteralPath $safe -Recurse -Force | ForEach-Object { SafePath $_.FullName }
        Remove-Item -LiteralPath $safe -Recurse -Force
    }
}
function Assert-Closed {
    $running=@(Get-Process -ErrorAction SilentlyContinue | Where-Object {
        $_.ProcessName -match '^(AfterFX|AfterFX\.com|aerender|aerendercore|Adobe Premiere Pro)$'
    })
    if ($running.Count) { throw 'Save your projects and fully close After Effects, Premiere Pro and Adobe render processes, then run setup again. No application will be closed automatically.' }
}
function Read-Manifest([string]$Root,[bool]$VerifyFiles) {
    $file=Contained (Join-Path $Root 'install-manifest.json') $Root
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw 'The package manifest is missing. Download and extract the complete release.' }
    $manifest=Get-Content -LiteralPath $file -Raw | ConvertFrom-Json
    if ($manifest.productId -ne $productId -or $manifest.schemaVersion -ne 1 -or $manifest.version -ne '1.0.0') { throw 'This is not a recognized 4x4-Tools v1.0 package.' }
    $seen=@{}
    foreach ($record in @($manifest.files)) {
        $relative=[string]$record.path
        if ($allowed -cnotcontains $relative -or $seen.ContainsKey($relative) -or $record.sha256 -notmatch '^[0-9a-fA-F]{64}$') {
            throw 'The installation manifest contains an invalid or duplicate file.'
        }
        $seen[$relative]=$true
        $path=Contained (Join-Path $Root $relative) $Root
        if ($VerifyFiles) {
            if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Package file is missing: $relative. Extract the full ZIP or download setup again." }
            if ((Hash $path) -ne $record.sha256) { throw "Package integrity check failed: $relative. Download a fresh release; the existing installation was not changed." }
        }
    }
    if ($seen.Count -ne $allowed.Count) { throw 'The installation manifest is incomplete.' }
    return $manifest
}
function Check-Gpu([string]$Root) {
    Log 'Checking GPU compatibility by rendering a small neural frame...'
    $check=Contained (Join-Path $Root 'SupportCheck.exe') $Root
    $start=New-Object Diagnostics.ProcessStartInfo
    $start.FileName=$check; $start.WorkingDirectory=$Root
    $start.UseShellExecute=$false; $start.CreateNoWindow=$true
    $start.RedirectStandardOutput=$true; $start.RedirectStandardError=$true
    $taskPath=[Environment]::GetEnvironmentVariable('Path','Process')
    foreach ($key in @($start.EnvironmentVariables.Keys)) {
        if ([string]$key -ieq 'Path') { $start.EnvironmentVariables.Remove([string]$key) }
    }
    $start.EnvironmentVariables['Path']=$taskPath
    $start.EnvironmentVariables['LOCALAPPDATA']=Join-Path $logDir ('gpu-'+$session)
    $process=New-Object Diagnostics.Process; $process.StartInfo=$start
    try {
        [void]$process.Start()
        $stdout=$process.StandardOutput.ReadToEndAsync(); $stderr=$process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit(60000)) {
            # Kill only the checker that this installer started, never an Adobe process.
            $process.Kill(); $process.WaitForExit()
            throw 'The GPU compatibility test timed out after 60 seconds. Restart Windows, update the NVIDIA driver, close GPU-intensive applications and try again. No plug-in files were replaced.'
        }
        $output=$stdout.Result.Trim(); $errors=$stderr.Result.Trim()
        if ($output) { Log $output }; if ($errors) { Log $errors }
        $result=$null
        try { $result=($output -split '\r?\n' | Select-Object -Last 1) | ConvertFrom-Json } catch { }
        if ($process.ExitCode -ne 0 -or -not $result -or -not $result.ok) {
            $detail='The checker stopped unexpectedly (exit '+$process.ExitCode+').'
            if ($result -and $result.message) { $detail=$result.message }
            throw ($detail+' The bundled runtime is not compatible with this GPU/driver configuration. Update the NVIDIA driver and retry. RTX 30/40/50 support is determined by this test; no alternate model is downloaded automatically. Your previous installation is unchanged.')
        }
        Log ('GPU test passed: '+$result.gpu)
        return $result
    } finally { $process.Dispose() }
}
function Install-Payload([string]$Source,$Manifest) {
    $stage=Contained (Join-Path $stateRoot ('staging-'+$session)) $stateRoot
    $backup=Contained (Join-Path $stateRoot ('Backups\'+$session)) $stateRoot
    $hadPlugin=Test-Path -LiteralPath $target
    $hadMaintenance=Test-Path -LiteralPath $maintenance
    $oldMoved=$false; $newMoved=$false; $maintenanceMoved=$false; $maintenanceInstalled=$false
    $oldRegistry=$null; $registryWritten=$false
    if (-not $SandboxRoot -and (Test-Path -LiteralPath $registryKey)) { $oldRegistry=Get-ItemProperty -LiteralPath $registryKey }
    New-Item -ItemType Directory -Force -Path $stage,(Join-Path $stage 'plugin'),(Join-Path $stage 'maintenance'),$backup | Out-Null
    try {
        foreach ($relative in @($allowed)+@('install-manifest.json')) {
            $destination=Contained (Join-Path (Join-Path $stage 'plugin') $relative) $stage
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destination) | Out-Null
            Copy-Item -LiteralPath (Join-Path $Source $relative) -Destination $destination
        }
        [void](Read-Manifest (Join-Path $stage 'plugin') $true)
        Copy-Item -LiteralPath $PSCommandPath -Destination (Join-Path $stage 'maintenance\installer-engine.ps1')
        if ($MaintenanceDir) {
            $uninstaller=Contained (Join-Path (Full $MaintenanceDir) 'Uninstall.exe') (Full $MaintenanceDir)
            if (-not (Test-Path -LiteralPath $uninstaller -PathType Leaf)) { throw 'The setup uninstaller is missing.' }
            Copy-Item -LiteralPath $uninstaller -Destination (Join-Path $stage 'maintenance\Uninstall.exe')
        }
        # Recheck immediately before changing live files.
        if (-not $SandboxRoot) { Assert-Closed }
        SafePath $target; SafePath $maintenance
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target),(Split-Path -Parent $maintenance) | Out-Null
        if ($hadPlugin) {
            Get-ChildItem -LiteralPath $target -Recurse -Force | ForEach-Object { SafePath $_.FullName }
            [void](Contained (Join-Path $backup 'plugin') $stateRoot)
            Move-Item -LiteralPath $target -Destination (Join-Path $backup 'plugin'); $oldMoved=$true
        }
        Move-Item -LiteralPath (Join-Path $stage 'plugin') -Destination $target; $newMoved=$true
        if ($hadMaintenance) {
            Get-ChildItem -LiteralPath $maintenance -Recurse -Force | ForEach-Object { SafePath $_.FullName }
            Move-Item -LiteralPath $maintenance -Destination (Join-Path $backup 'maintenance'); $maintenanceMoved=$true
        }
        Move-Item -LiteralPath (Join-Path $stage 'maintenance') -Destination $maintenance; $maintenanceInstalled=$true
        if (-not $SandboxRoot) {
            New-Item -Path $registryKey -Force | Out-Null; $registryWritten=$true
            $uninstall='"'+(Join-Path $maintenance 'Uninstall.exe')+'"'
            if (-not $MaintenanceDir) {
                $uninstall='"'+(Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe')+'" -NoProfile -ExecutionPolicy Bypass -File "'+(Join-Path $maintenance 'installer-engine.ps1')+'" -Action Uninstall'
            }
            foreach ($pair in @{DisplayName='4x4-Tools DLSS 5';DisplayVersion='1.0.0';Publisher='4x4-Tools';InstallLocation=$target;UninstallString=$uninstall;URLInfoAbout='https://github.com/Insider4444/4x4-Tools-DLSS-5'}.GetEnumerator()) {
                New-ItemProperty -LiteralPath $registryKey -Name $pair.Key -Value $pair.Value -PropertyType String -Force | Out-Null
            }
            New-ItemProperty -LiteralPath $registryKey -Name NoModify -Value 1 -PropertyType DWord -Force | Out-Null
            New-ItemProperty -LiteralPath $registryKey -Name NoRepair -Value 1 -PropertyType DWord -Force | Out-Null
            New-ItemProperty -LiteralPath $registryKey -Name EstimatedSize -Value 175000 -PropertyType DWord -Force | Out-Null
        }
        Log ('Installed v1.0 into '+$target)
        if ($oldMoved -or $maintenanceMoved) { Log ('Previous installation preserved at '+$backup) }
    } catch {
        $cause=$_
        Log 'Installation failed; restoring the previous installation.'
        if ($newMoved) { Remove-OwnedTree $target (Split-Path -Parent $target) }
        if ($oldMoved) { Move-Item -LiteralPath (Contained (Join-Path $backup 'plugin') $stateRoot) -Destination $target }
        if ($maintenanceInstalled) { Remove-OwnedTree $maintenance (Split-Path -Parent $maintenance) }
        if ($maintenanceMoved) { Move-Item -LiteralPath (Contained (Join-Path $backup 'maintenance') $stateRoot) -Destination $maintenance }
        if ($registryWritten) {
            Remove-Item -LiteralPath $registryKey -Force
            if ($oldRegistry) {
                New-Item -Path $registryKey -Force | Out-Null
                foreach ($property in $oldRegistry.PSObject.Properties) {
                    if ($property.Name -notmatch '^PS') { Set-ItemProperty -LiteralPath $registryKey -Name $property.Name -Value $property.Value }
                }
            }
        }
        throw $cause
    } finally { Remove-OwnedTree $stage $stateRoot }
}
function Uninstall-Payload {
    if (-not (Test-Path -LiteralPath $target)) { Log 'The plug-in is already absent.' }
    else {
        $manifest=Read-Manifest $target $false
        foreach ($record in @($manifest.files)) {
            $file=Contained (Join-Path $target $record.path) $target
            if (Test-Path -LiteralPath $file -PathType Leaf) {
                if ((Hash $file) -eq $record.sha256) { Remove-Item -LiteralPath $file -Force }
                else { Log ('Preserved a modified file: '+$file) }
            }
        }
        Remove-Item -LiteralPath (Contained (Join-Path $target 'install-manifest.json') $target) -Force
        $runtime=Contained (Join-Path $target 'runtime') $target
        if ((Test-Path -LiteralPath $runtime) -and @(Get-ChildItem -LiteralPath $runtime -Force).Count -eq 0) { Remove-Item -LiteralPath $runtime }
        if (@(Get-ChildItem -LiteralPath $target -Force).Count -eq 0) { Remove-Item -LiteralPath $target }
        else { Log ('Unrecognized or modified files were left in '+$target) }
    }
    if (-not $SandboxRoot -and (Test-Path -LiteralPath $registryKey)) { Remove-Item -LiteralPath $registryKey -Force }
    if (-not (Test-Path -LiteralPath (Join-Path $maintenance 'Uninstall.exe'))) {
        $installedScript=Contained (Join-Path $maintenance 'installer-engine.ps1') $maintenance
        if (Test-Path -LiteralPath $installedScript) { Remove-Item -LiteralPath $installedScript -Force }
        if ((Test-Path -LiteralPath $maintenance) -and @(Get-ChildItem -LiteralPath $maintenance -Force).Count -eq 0) { Remove-Item -LiteralPath $maintenance }
    }
    Log 'Plug-in uninstalled. Backups and diagnostic logs are preserved.'
}
try {
    $mutexName=if ($SandboxRoot) { 'Local\4x4-Tools-DLSS-5-Installer-Test' } else { 'Global\4x4-Tools-DLSS-5-Setup' }
    $installMutex=New-Object Threading.Mutex($false,$mutexName)
    try { $hasMutex=$installMutex.WaitOne(0) } catch [Threading.AbandonedMutexException] { $hasMutex=$true }
    if (-not $hasMutex) { throw 'Another 4x4-Tools setup or compatibility check is running. Wait for it to finish before trying again.' }
    if (-not [Environment]::Is64BitOperatingSystem -or -not [Environment]::Is64BitProcess) { throw 'Use the 64-bit edition of Windows and Windows PowerShell.' }
    if ($SandboxRoot) {
        $SandboxRoot=Full $SandboxRoot; SafePath $SandboxRoot
        if (-not (Test-Path -LiteralPath (Join-Path $SandboxRoot '.4x4-installer-test-root') -PathType Leaf)) { throw 'A marked installer test directory is required for SandboxRoot.' }
        $target=Contained (Join-Path $SandboxRoot 'MediaCore\4x4Tools-DLSS5') $SandboxRoot
        $maintenance=Contained (Join-Path $SandboxRoot 'Maintenance') $SandboxRoot
        $stateRoot=Contained (Join-Path $SandboxRoot 'State') $SandboxRoot
    } else {
        $programFiles=[Environment]::GetFolderPath('ProgramFiles')
        $target=Join-Path $programFiles 'Adobe\Common\Plug-ins\7.0\MediaCore\4x4Tools-DLSS5'
        $maintenance=Join-Path $programFiles '4x4-Tools\DLSS-5'
        $stateRoot=Join-Path ([Environment]::GetFolderPath('CommonApplicationData')) '4x4-Tools\DLSS-5'
        if ($Action -eq 'Validate') { $stateRoot=Join-Path ([IO.Path]::GetTempPath()) '4x4-Tools-DLSS-5-Validation' }
        else {
            $identity=[Security.Principal.WindowsIdentity]::GetCurrent()
            if (-not (New-Object Security.Principal.WindowsPrincipal($identity)).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { throw 'Run the installer as administrator to update the shared Adobe plug-in folder.' }
            Assert-Closed
        }
    }
    SafePath $target; SafePath $maintenance; SafePath $stateRoot
    $logDir=Join-Path $stateRoot 'Logs'; SafePath $logDir
    New-Item -ItemType Directory -Force -Path $logDir | Out-Null
    $logPath=Join-Path $logDir ($Action+'-'+$session+'.log')
    Log ('4x4-Tools v1.0 '+$Action)
    if ($Action -eq 'Uninstall') { Uninstall-Payload }
    else {
        $PackageDir=Full $PackageDir; SafePath $PackageDir
        $manifest=Read-Manifest $PackageDir $true
        Log 'Package integrity verified.'
        if ($Action -eq 'Install' -and -not $SandboxRoot) {
            $adobe=Join-Path ([Environment]::GetFolderPath('ProgramFiles')) 'Adobe'
            $hosts=@(Get-ChildItem -LiteralPath $adobe -Directory -ErrorAction SilentlyContinue | Where-Object {
                (Test-Path -LiteralPath (Join-Path $_.FullName 'Support Files\AfterFX.exe')) -or (Test-Path -LiteralPath (Join-Path $_.FullName 'Adobe Premiere Pro.exe'))
            })
            # Custom Creative Cloud locations can still use the canonical shared MediaCore directory.
            $installedAdobe=@(Get-ChildItem 'HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall' -ErrorAction SilentlyContinue | Get-ItemProperty -ErrorAction SilentlyContinue | Where-Object {
                $_.PSObject.Properties['DisplayName'] -and $_.DisplayName -match 'Adobe (After Effects|Premiere Pro)'
            })
            if (-not $hosts.Count -and -not $installedAdobe.Count) { throw 'Install After Effects or Premiere Pro before installing this plug-in.' }
        }
        [void](Check-Gpu $PackageDir)
        if ($Action -eq 'Install') { Install-Payload $PackageDir $manifest }
        else { Log 'Validation passed. No Adobe plug-in files were changed.' }
    }
    Log ('Log: '+$logPath)
    exit 0
} catch {
    $failure=$_
    if (-not $logPath) {
        try {
            $earlyDir=Join-Path ([IO.Path]::GetTempPath()) '4x4-Tools-DLSS-5-Validation\Logs'
            SafePath $earlyDir
            New-Item -ItemType Directory -Force -Path $earlyDir | Out-Null
            $logPath=Join-Path $earlyDir ($Action+'-'+$session+'.log')
        } catch { Write-Host 'A setup log could not be created.' }
    }
    Log ('ERROR: '+$failure.Exception.Message)
    if ($logPath) { Write-Host ('Log: '+$logPath) }
    exit 1
} finally {
    if ($hasMutex -and $installMutex) { $installMutex.ReleaseMutex() }
    if ($installMutex) { $installMutex.Dispose() }
}
