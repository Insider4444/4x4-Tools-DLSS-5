#Requires -Version 5.1
[CmdletBinding()]
param([string]$MakeNsis,[switch]$PayloadOnly)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'common.ps1')
$info=Sync-ReleaseVersion; $config=$info.Config
$binary=Join-Path $ProjectRoot 'build\Release'
$receipt=Get-Content -LiteralPath (Join-Path $ProjectRoot 'build\build-receipt.json') -Raw | ConvertFrom-Json
$fingerprint=Get-SourceFingerprint -CodeOnly
if ($receipt.version -ne $config.version -or $receipt.coreFingerprint -ne $fingerprint -or $receipt.cpuOnly) { throw 'Source/version changed or only CPU tests were built. Run scripts/build.ps1 before packaging.' }
foreach ($record in $receipt.binaries) { if ((Get-Sha (Join-Path $binary $record.path)) -ne $record.sha256) { throw 'Build output changed since testing. Rebuild first.' } }
$stamp=$config.version+'-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,8)
$stage=Join-Path $ProjectRoot ('package\'+$stamp); $payload=Join-Path $stage 'payload'
New-Item -ItemType Directory -Force -Path (Join-Path $payload 'runtime') | Out-Null
foreach ($file in @('4x4Tools-DLSS5.aex','SupportCheck.exe','UpdateCheck.exe','runtime\nvngx_dlssnr.dll')) {
    Copy-Item -LiteralPath (Join-Path $binary $file) -Destination (Join-Path $payload $file)
}
if ((Get-Sha (Join-Path $payload 'runtime\nvngx_dlssnr.dll')) -ne '984bee0f775c277d5829b8fd6775d53a7b0f75396c852b3aaf06a18375f81014') { throw 'Runtime differs from the reviewed binary. Update provenance, licensing and GPU validation before changing it.' }
$copies=@{'LICENSE'='LICENSE.txt';'THIRD-PARTY-NOTICES.md'='THIRD-PARTY-NOTICES.txt';'licenses\NVIDIA-RTX-SDK.txt'='NVIDIA-RTX-SDK.txt';'licenses\UPSTREAM-MIT.txt'='UPSTREAM-MIT.txt';'docs\CONTROLS.md'='CONTROLS.md'}
foreach ($pair in $copies.GetEnumerator()) { Copy-Item -LiteralPath (Join-Path $ProjectRoot $pair.Key) -Destination (Join-Path $payload $pair.Value) }
$files=@('4x4Tools-DLSS5.aex','SupportCheck.exe','UpdateCheck.exe','runtime/nvngx_dlssnr.dll','LICENSE.txt','THIRD-PARTY-NOTICES.txt','NVIDIA-RTX-SDK.txt','UPSTREAM-MIT.txt','CONTROLS.md')
$records=@($files | ForEach-Object { @{path=$_;sha256=(Get-Sha (Join-Path $payload $_))} })
Write-JsonFile @{schemaVersion=1;productId='4x4-Tools-DLSS-5';version=$config.version;adobeCompatibilityVersion=$info.AdobeVersion;files=$records} (Join-Path $payload 'install-manifest.json')
$result=@{version=$config.version;packageDir=$stage;coreFingerprint=$fingerprint;created=(Get-Date -Format o)}
if (-not $PayloadOnly) {
    if (-not $MakeNsis) { $MakeNsis=(Read-BuildSettings).MakeNsis }
    $dist=Join-Path $ProjectRoot ('dist\'+$stamp); New-Item -ItemType Directory -Path $dist | Out-Null
    $base=$config.releaseName+'-v'+$config.version
    $exe=Join-Path $dist ($base+'-Setup.exe'); $zipFile=Join-Path $dist ($base+'.zip')
    $licenseText=@(('4x4Tools-DLSS5-win v'+$config.version),'Independent software. NVIDIA and Adobe do not endorse this plug-in.','The plug-in source is MIT. The bundled community-modified NVIDIA runtime has separate terms.')
    foreach ($license in @('LICENSE','THIRD-PARTY-NOTICES.md','licenses\NVIDIA-RTX-SDK.txt','licenses\UPSTREAM-MIT.txt','licenses\NSIS.txt')) { $licenseText+=Get-Content -LiteralPath (Join-Path $ProjectRoot $license) -Raw -Encoding UTF8 }
    [IO.File]::WriteAllText((Join-Path $stage 'SETUP-LICENSE.txt'),($licenseText -join "`r`n`r`n"),[Text.Encoding]::Unicode)
    [void](Invoke-Native $MakeNsis @('/V3',"/DPACKAGE=$stage","/DOUTPUT=$exe","/DVERSION=$($config.version)",(Join-Path $ProjectRoot 'installer\setup.nsi')) -LogPath (Join-Path $stage 'nsis.log'))
    New-ManualZip $payload $zipFile $config.version
    $sums=Join-Path $dist 'SHA256SUMS.txt'
    [IO.File]::WriteAllLines($sums,@($exe,$zipFile | ForEach-Object { (Get-Sha $_)+'  '+[IO.Path]::GetFileName($_) }),[Text.Encoding]::ASCII)
    $result.exe=$exe; $result.zip=$zipFile; $result.dist=$dist; $result.checksums=$sums
}
Write-JsonFile $result (Join-Path $ProjectRoot 'build\package-result.json')
Write-Host ('Package ready: '+$stage)
