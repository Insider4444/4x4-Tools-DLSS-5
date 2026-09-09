#Requires -Version 5.1
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\scripts\common.ps1')
$package=Get-Content (Join-Path $ProjectRoot 'build\package-result.json') -Raw | ConvertFrom-Json
$root=Join-Path $ProjectRoot ('artifacts\package-layout-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root -Force | Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip=[IO.Compression.ZipFile]::OpenRead($package.zip)
try {
    $entries=@($zip.Entries | ForEach-Object { $_.FullName })
    $manifest=Get-Content (Join-Path $package.packageDir 'payload\install-manifest.json') -Raw | ConvertFrom-Json
    $expected=@('README.txt','4x4Tools-DLSS5/install-manifest.json')+@($manifest.files | ForEach-Object { '4x4Tools-DLSS5/'+$_.path })
    if ($entries.Count -ne $expected.Count -or @(Compare-Object $entries $expected).Count) { throw 'Unexpected public ZIP layout.' }
} finally { $zip.Dispose() }
Expand-Archive -LiteralPath $package.zip -DestinationPath $root
foreach ($file in $manifest.files) { if ((Get-Sha (Join-Path $root ('4x4Tools-DLSS5/'+$file.path))) -ne $file.sha256) { throw 'ZIP payload integrity failure.' } }
if ((Get-Item $package.exe).VersionInfo.ProductVersion -ne $package.version) { throw 'Installer version does not match release configuration.' }
foreach ($asset in @($package.exe,$package.zip)) {
    $line=(Get-Sha $asset)+'  '+[IO.Path]::GetFileName($asset)
    if (@(Get-Content $package.checksums) -cnotcontains $line) { throw 'Published checksum differs from the asset.' }
}
Write-Host 'PASS manual ZIP allowlist, every payload hash, installer version and release checksums'
