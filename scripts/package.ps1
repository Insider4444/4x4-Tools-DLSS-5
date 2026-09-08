#Requires -Version 5.1
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$MakeNsis)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$binary=Join-Path $projectRoot 'build\Release'
$stamp=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,8)
$stage=Join-Path $projectRoot ('package\'+$stamp)
$payload=Join-Path $stage 'payload'
$dist=Join-Path $projectRoot 'dist'
New-Item -ItemType Directory -Force -Path $payload,(Join-Path $payload 'runtime'),$dist | Out-Null
foreach ($file in @('4x4Tools-DLSS5.aex','SupportCheck.exe','runtime\nvngx_dlssnr.dll')) {
    if (-not (Test-Path -LiteralPath (Join-Path $binary $file))) { throw "Build first. Missing $file." }
    Copy-Item -LiteralPath (Join-Path $binary $file) -Destination (Join-Path $payload $file)
}
$expectedRuntime='984BEE0F775C277D5829B8FD6775D53A7B0F75396C852B3AAF06A18375F81014'
if ((Get-FileHash -LiteralPath (Join-Path $payload 'runtime\nvngx_dlssnr.dll')).Hash -ne $expectedRuntime) {
    throw 'Runtime differs from the reviewed v1.0 binary. Update provenance, licensing and GPU validation before changing the package.'
}
$copies=@{
    'LICENSE'='LICENSE.txt'; 'THIRD-PARTY-NOTICES.md'='THIRD-PARTY-NOTICES.txt';
    'licenses\NVIDIA-RTX-SDK.txt'='NVIDIA-RTX-SDK.txt'; 'licenses\UPSTREAM-MIT.txt'='UPSTREAM-MIT.txt'; 'docs\CONTROLS.md'='CONTROLS.md'
}
foreach ($pair in $copies.GetEnumerator()) { Copy-Item -LiteralPath (Join-Path $projectRoot $pair.Key) -Destination (Join-Path $payload $pair.Value) }
$files=@('4x4Tools-DLSS5.aex','SupportCheck.exe','runtime/nvngx_dlssnr.dll','LICENSE.txt','THIRD-PARTY-NOTICES.txt','NVIDIA-RTX-SDK.txt','UPSTREAM-MIT.txt','CONTROLS.md')
$records=@($files | ForEach-Object { @{path=$_;sha256=(Get-FileHash -LiteralPath (Join-Path $payload $_)).Hash} })
@{schemaVersion=1;productId='4x4-Tools-DLSS-5';version='1.0.0';adobeCompatibilityVersion='1.3.0';files=$records} |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $payload 'install-manifest.json') -Encoding UTF8
foreach ($name in @('installer-engine.ps1','Install.ps1','Uninstall.ps1')) {
    Copy-Item -LiteralPath (Join-Path $projectRoot ('installer\'+$name)) -Destination (Join-Path $stage $name)
}
Copy-Item -LiteralPath (Join-Path $projectRoot 'docs\INSTALLATION.md') -Destination (Join-Path $stage 'INSTALLATION.md')
Copy-Item -LiteralPath (Join-Path $projectRoot 'docs\COMPATIBILITY.md') -Destination (Join-Path $stage 'COMPATIBILITY.md')
Copy-Item -LiteralPath (Join-Path $projectRoot 'licenses\NSIS.txt') -Destination (Join-Path $stage 'NSIS-LICENSE.txt')
@'
4x4-Tools DLSS 5 v1.0 - Windows x64

Save and close After Effects and Premiere Pro before installation.
Run Install.ps1 from Windows PowerShell, or use the guided EXE from Releases.
Install.ps1 -ValidateOnly checks the package and GPU without installing.
Uninstall.ps1 removes the plug-in. Read INSTALLATION.md and COMPATIBILITY.md.
The source plug-in is MIT; the included neural runtime has separate terms.
Read payload/THIRD-PARTY-NOTICES.txt and payload/NVIDIA-RTX-SDK.txt.

Download, source and support:
https://github.com/Insider4444/4x4-Tools-DLSS-5
'@ | Set-Content -LiteralPath (Join-Path $stage 'README.txt') -Encoding UTF8
$licenseText=@('4x4-Tools DLSS 5 v1.0',
    'Independent software. NVIDIA and Adobe do not endorse this plug-in.',
    'The plug-in source is MIT. The bundled community-modified NVIDIA neural runtime is separately licensed.',
    'Read all applicable third-party terms below before installing.',
    (Get-Content -LiteralPath (Join-Path $projectRoot 'LICENSE') -Raw -Encoding UTF8),
    (Get-Content -LiteralPath (Join-Path $projectRoot 'THIRD-PARTY-NOTICES.md') -Raw -Encoding UTF8),
    (Get-Content -LiteralPath (Join-Path $projectRoot 'licenses\NVIDIA-RTX-SDK.txt') -Raw -Encoding UTF8),
    (Get-Content -LiteralPath (Join-Path $projectRoot 'licenses\UPSTREAM-MIT.txt') -Raw -Encoding UTF8),
    (Get-Content -LiteralPath (Join-Path $projectRoot 'licenses\NSIS.txt') -Raw -Encoding UTF8)) -join "`r`n`r`n"
# NSIS recognizes the BOM and displays non-ASCII license punctuation correctly.
$licenseText | Set-Content -LiteralPath (Join-Path $stage 'SETUP-LICENSE.txt') -Encoding Unicode
$exe=Join-Path $dist '4x4-Tools-DLSS-5-v1.0-Setup.exe'
$zip=Join-Path $dist '4x4-Tools-DLSS-5-v1.0-Windows-x64.zip'
if ((Test-Path -LiteralPath $exe) -or (Test-Path -LiteralPath $zip)) {
    throw 'dist already contains release artifacts. Preserve or relocate them before packaging again.'
}
& $MakeNsis '/V3' "/DPACKAGE=$stage" "/DOUTPUT=$exe" (Join-Path $projectRoot 'installer\setup.nsi')
if ($LASTEXITCODE -ne 0) { throw 'NSIS installer compilation failed.' }
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -CompressionLevel Optimal
@($exe,$zip) | ForEach-Object { (Get-FileHash -LiteralPath $_).Hash.ToLowerInvariant()+'  '+[IO.Path]::GetFileName($_) } |
    Set-Content -LiteralPath (Join-Path $dist 'SHA256SUMS.txt') -Encoding ASCII
@{packageDir=$stage;exe=$exe;zip=$zip;created=(Get-Date -Format o)} | ConvertTo-Json |
    Set-Content -LiteralPath (Join-Path $projectRoot 'build\package-result.json') -Encoding UTF8
Write-Host "Packaged EXE and ZIP in $dist"
