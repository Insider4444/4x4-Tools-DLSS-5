#Requires -Version 5.1
[CmdletBinding()]
param([string]$AdobeSdk,[string]$NgxSdk,[string]$Runtime,[string]$MakeNsis)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'scripts\common.ps1')
$config=Read-ReleaseConfig
if (-not (Test-Path -LiteralPath (Join-Path $PSScriptRoot '.git'))) {
    [void](Git @('init','-b','main'))
    $bundle=Join-Path $PSScriptRoot 'repository.bundle'
    if (Test-Path -LiteralPath $bundle) {
        [void](Git @('bundle','verify',$bundle))
        [void](Git @('fetch',$bundle,'refs/heads/main:refs/remotes/origin/main','refs/tags/*:refs/tags/*'))
        $head=(Git @('rev-parse','refs/remotes/origin/main')).Text.Trim()
        [void](Git @('update-ref','refs/heads/main',$head))
        [void](Git @('reset','--mixed','HEAD')) # Populate the new index; preserve every extracted/edited working file.
    }
    [void](Git @('remote','add','origin',('https://github.com/'+$config.repository+'.git')))
    Write-Host 'Git history restored. Your working files were preserved.'
}
$file=Join-Path $PSScriptRoot '.local\build-settings.json'
$settings=[pscustomobject]@{AdobeSdk='dependencies/ae-sdk';NgxSdk='dependencies/nvidia-dlss';Runtime='dependencies/runtime/nvngx_dlssnr.dll';MakeNsis='dependencies/nsis/makensis.exe'}
if (Test-Path -LiteralPath $file) { $settings=Get-Content -LiteralPath $file -Raw -Encoding UTF8 | ConvertFrom-Json }
foreach ($field in @('AdobeSdk','NgxSdk','Runtime','MakeNsis')) {
    $value=Get-Variable -Name $field -ValueOnly
    if ($value) { $settings.$field=$value }
}
Write-JsonFile $settings $file
[void](Read-BuildSettings)
[void](Find-Tool 'git')
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw 'Install Visual Studio 2022/2026 Build Tools with Desktop development with C++ and a Windows SDK. The private kit supplies Adobe/NVIDIA SDKs and NSIS, not the Microsoft compiler.'
}
Write-Host 'Developer paths are ready. Run debug-install.ps1 after closing Adobe. GitHub CLI authentication is needed only when publishing.'
