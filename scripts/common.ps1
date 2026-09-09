#Requires -Version 5.1
$script:ProjectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
function Write-JsonFile($Value,[string]$Path) {
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($Path)) | Out-Null
    [IO.File]::WriteAllText($Path,($Value | ConvertTo-Json -Depth 12)+"`n",[Text.UTF8Encoding]::new($false))
}
function Get-Sha([string]$Path) {
    $stream=[IO.File]::OpenRead($Path); $sha=[Security.Cryptography.SHA256]::Create()
    try { [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','').ToLowerInvariant() }
    finally { $sha.Dispose(); $stream.Dispose() }
}
function Get-TextSha([string]$Text) {
    $sha=[Security.Cryptography.SHA256]::Create()
    try { [BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($Text))).Replace('-','').ToLowerInvariant() }
    finally { $sha.Dispose() }
}
function Get-LocalPath([string]$Path) {
    if (-not [IO.Path]::IsPathRooted($Path)) { $Path=Join-Path $script:ProjectRoot $Path }
    [IO.Path]::GetFullPath($Path)
}
function Quote-Native([string]$Value) {
    '"'+[regex]::Replace([regex]::Replace($Value,'(\\*)"','$1$1\"'),'(\\+)$','$1$1')+'"'
}
function Find-Tool([string]$Name) {
    $found=Get-Command $Name -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) { return $found.Source }
    $fallback=@{git='Git\cmd\git.exe';gh='GitHub CLI\gh.exe'}
    if ($fallback.ContainsKey($Name)) {
        $file=Join-Path ([Environment]::GetFolderPath('ProgramFiles')) $fallback[$Name]
        if (Test-Path -LiteralPath $file) { return $file }
    }
    throw "Install $Name and reopen PowerShell before continuing."
}
function Invoke-Native([string]$Exe,[string[]]$Arguments,[int]$TimeoutSeconds=600,[switch]$AllowFailure,[string]$LogPath,[string]$InputFile) {
    $start=New-Object Diagnostics.ProcessStartInfo
    $start.FileName=$Exe; $start.WorkingDirectory=$script:ProjectRoot
    $start.UseShellExecute=$false; $start.CreateNoWindow=$true
    $start.RedirectStandardOutput=$true; $start.RedirectStandardError=$true
    $start.RedirectStandardInput=[bool]$InputFile
    $start.Arguments=($Arguments | ForEach-Object { Quote-Native $_ }) -join ' '
    $taskPath=[Environment]::GetEnvironmentVariable('Path','Process')
    foreach ($key in @($start.EnvironmentVariables.Keys)) { if ([string]$key -ieq 'Path') { $start.EnvironmentVariables.Remove([string]$key) } }
    try { $taskPath=(Split-Path -Parent (Find-Tool 'gh'))+';'+$taskPath } catch { }
    $start.EnvironmentVariables['Path']=$taskPath
    $process=New-Object Diagnostics.Process; $process.StartInfo=$start
    try {
        [void]$process.Start()
        $stdout=$process.StandardOutput.ReadToEndAsync(); $stderr=$process.StandardError.ReadToEndAsync()
        if ($InputFile) { $process.StandardInput.Write([IO.File]::ReadAllText($InputFile)); $process.StandardInput.Close() }
        if (-not $process.WaitForExit($TimeoutSeconds*1000)) { $process.Kill(); throw ('Timed out: '+[IO.Path]::GetFileName($Exe)) }
        $result=[pscustomobject]@{Code=$process.ExitCode;Text=$stdout.Result;Error=$stderr.Result}
        if ($LogPath) { [IO.File]::WriteAllText($LogPath,$result.Text+$result.Error,[Text.UTF8Encoding]::new($false)) }
        if ($result.Code -ne 0 -and -not $AllowFailure) { throw ([IO.Path]::GetFileName($Exe)+' failed: '+$result.Error+$result.Text) }
        return $result
    } finally { $process.Dispose() }
}
function Git([string[]]$Arguments,[switch]$AllowFailure) {
    Invoke-Native (Find-Tool 'git') (@('-c',"safe.directory=$script:ProjectRoot",'-c','credential.helper=','-c','credential.helper=!gh auth git-credential')+$Arguments) -AllowFailure:$AllowFailure
}
function Gh([string[]]$Arguments,[switch]$AllowFailure) { Invoke-Native (Find-Tool 'gh') $Arguments -AllowFailure:$AllowFailure }
function Read-ReleaseConfig([string]$Version) {
    $path=Join-Path $script:ProjectRoot 'release-config.json'
    $config=Get-Content -LiteralPath $path -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($Version) { $config.version=$Version.TrimStart('v') }
    if ($config.version -notmatch '^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$') { throw 'Set version to three numbers such as 1.1.0 in release-config.json.' }
    foreach ($part in $config.version.Split('.')) { if ([int64]$part -gt 65535) { throw 'Version components must be between 0 and 65535.' } }
    if ($config.repository -notmatch '^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$' -or $config.releaseName -notmatch '^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$') { throw 'Invalid repository or releaseName in release-config.json.' }
    if ($Version) { Write-JsonFile $config $path }
    return $config
}
function Sync-ReleaseVersion {
    $config=Read-ReleaseConfig
    $file=Join-Path $script:ProjectRoot 'resources\version-state.json'
    $state=Get-Content -LiteralPath $file -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($state.forVersion -ne $config.version) {
        if ([version]$config.version -le [version]$state.forVersion) { throw 'Choose a version greater than the previous development version.' }
        $state.revision=[int]$state.revision+1; $state.forVersion=$config.version
        Write-JsonFile $state $file
    }
    if ([int]$state.revision -lt 24577 -or [int]$state.revision -ge 16646144) { throw 'Adobe version counter is outside its supported range.' }
    $revision=[int]$state.revision
    [pscustomobject]@{Config=$config;Revision=$revision;AdobeVersion=('{0}.{1}.{2}.{3}' -f (1+[math]::Floor($revision/131072)),([math]::Floor($revision/8192)%16),([math]::Floor($revision/512)%16),($revision%512))}
}
function Read-BuildSettings {
    $file=Join-Path $script:ProjectRoot '.local\build-settings.json'
    if (-not (Test-Path -LiteralPath $file)) { throw 'Run setup-dev.ps1 to configure the local SDK and installer-tool paths.' }
    $settings=Get-Content -LiteralPath $file -Raw -Encoding UTF8 | ConvertFrom-Json
    foreach ($field in @('AdobeSdk','NgxSdk','Runtime','MakeNsis')) {
        $settings.$field=Get-LocalPath $settings.$field
        if (-not (Test-Path -LiteralPath $settings.$field)) { throw "Missing $field at $($settings.$field). Run setup-dev.ps1 with your local paths." }
    }
    return $settings
}
function Get-PublicFiles {
    $names=(Git @('ls-files','--cached','--others','--exclude-standard','-z')).Text.Split([char]0) | Where-Object { $_ }
    $files=@($names | Sort-Object -Unique | Where-Object { Test-Path -LiteralPath (Join-Path $script:ProjectRoot $_) -PathType Leaf })
    foreach ($file in $files) {
        if ($file -ne 'dependencies/README.md' -and $file -match '(^|/)(\.local|dependencies|external|build[^/]*|artifacts|package|dist|diagnostics|\.git)/|(^|/)(\.env[^/]*|PRIVATE-DEVELOPER-KIT.txt|repository.bundle)$|\.(dll|aex|exe|lib|zip|bundle|pdb|aep|prproj|avi|mp4|mov)$') { throw "Private/build file is tracked in Git: $file. Remove it from the index before publishing." }
        $item=Get-Item -LiteralPath (Join-Path $script:ProjectRoot $file) -Force
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Linked source file cannot be packaged: $file" }
        if ($item.Length -gt 5MB) { throw "Unexpected large source file: $file" }
        if ($file -notmatch '\.(ico|png|bmp)$') {
            $text=[IO.File]::ReadAllText($item.FullName)
            if ($text -match '(?:gh[pousr]_[A-Za-z0-9]{25,}|github_pat_[A-Za-z0-9_]{30,}|AKIA[A-Z0-9]{16}|-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----)') { throw "Possible credential material detected in $file. Review it before publishing." }
        }
    }
    return $files
}
function Get-SourceFingerprint([switch]$CodeOnly) {
    $files=@(Get-PublicFiles)
    if ($CodeOnly) { $files=@($files | Where-Object { $_ -match '^(src|installer|scripts|tests|cmake|resources)/|^CMakeLists.txt$|^(debug-install|deploy-main|publish-release|setup-dev)\.ps1$' -and $_ -ne 'resources/version-state.json' }) }
    $records=@($files | ForEach-Object { $_+':'+(Get-Sha (Join-Path $script:ProjectRoot $_)) })
    Get-TextSha ($records -join "`n")
}
function Assert-AdobeClosed {
    $apps=@(Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.ProcessName -match '^(AfterFX|AfterFX\.com|aerender|aerendercore|Adobe Premiere Pro)$' })
    if ($apps.Count) { throw 'Save and fully close After Effects, Premiere Pro and Adobe render processes before local installation.' }
}
function New-SourceZip([string]$Destination) {
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    if (Test-Path -LiteralPath $Destination) { throw 'Source ZIP already exists.' }
    $zip=[IO.Compression.ZipFile]::Open($Destination,[IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($file in @(Get-PublicFiles)) {
            [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip,(Join-Path $script:ProjectRoot $file),$file,[IO.Compression.CompressionLevel]::Optimal) | Out-Null
        }
    } finally { $zip.Dispose() }
}
function Get-ManualReadme([string]$Version) {
    return @"
4x4Tools-DLSS5-win v$Version - manual installation

1. Save your projects and fully close After Effects and Premiere Pro.
2. Run 4x4Tools-DLSS5\SupportCheck.exe from a terminal to test your GPU.
   Continue only if the last line reports ok: true.
3. Back up any existing 4x4Tools-DLSS5 folder OUTSIDE Adobe's plug-in folders.
4. Copy the entire 4x4Tools-DLSS5 folder into:
   C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\
   Accept the Windows administrator prompt. Keep the runtime subfolder intact.
5. Reopen Adobe and search Effects for 4x4Tools-DLSS5.

Use the installer EXE from Releases if you prefer guided setup and rollback.
This ZIP has only installed plug-in files, required helpers/runtime and documentation.
It does not contain source, SDKs or development scripts.
Manual installation does not create a Windows Apps uninstall entry.
To remove a manual installation, close Adobe and remove only its 4x4Tools-DLSS5 folder.

Requires Windows x64 and a compatible NVIDIA RTX GPU/driver. The GPU checker
determines compatibility; support across all RTX 30/40/50 cards is not guaranteed.
This is an experimental neural enhancement runtime, not NVIDIA's announced
DLSS 5 neural-rendering product. No upscaling or frame generation is performed.
Read CONTROLS.md and the separate runtime license files inside the plug-in folder.
Updates (v1.1+): a small status row shows newer stable releases. Use the Updates
menu to check manually, disable automatic checks, or open the release page.
No footage is sent and no update is installed automatically.

Downloads and support: https://github.com/Insider4444/4x4-Tools-DLSS-5
"@
}
function New-ManualZip([string]$Payload,[string]$Destination,[string]$Version) {
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $manifest=Get-Content -LiteralPath (Join-Path $Payload 'install-manifest.json') -Raw | ConvertFrom-Json
    $zip=[IO.Compression.ZipFile]::Open($Destination,[IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($file in @($manifest.files | ForEach-Object { $_.path })+@('install-manifest.json')) {
            if ($file -notmatch '^(4x4Tools-DLSS5\.aex|SupportCheck\.exe|UpdateCheck\.exe|runtime/nvngx_dlssnr\.dll|LICENSE\.txt|THIRD-PARTY-NOTICES\.txt|NVIDIA-RTX-SDK\.txt|UPSTREAM-MIT\.txt|CONTROLS\.md|install-manifest\.json)$') { throw 'Unexpected file in manual package.' }
            [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip,(Join-Path $Payload $file),('4x4Tools-DLSS5/'+$file),[IO.Compression.CompressionLevel]::Optimal) | Out-Null
        }
        $entry=$zip.CreateEntry('README.txt'); $writer=[IO.StreamWriter]::new($entry.Open(),[Text.UTF8Encoding]::new($false))
        try { $writer.Write((Get-ManualReadme $Version)) } finally { $writer.Dispose() }
    } finally { $zip.Dispose() }
}
