#Requires -Version 5.1
[CmdletBinding()]
param(
    [string]$AdobeSdk = $env:AE_SDK_BASE_PATH,
    [string]$NgxSdk = $env:DLSS_SDK_ROOT,
    [string]$Runtime = $env:DLSSNR_RUNTIME_DLL,
    [switch]$CpuOnly,
    [switch]$Photoshop = $true,
    [switch]$HostedCI
)
$ErrorActionPreference = 'Stop'
if ($HostedCI -and $env:GITHUB_ACTIONS -ne 'true') { throw 'HostedCI is reserved for the GitHub build runner. Local release validation requires the actual GPU tests.' }
. (Join-Path $PSScriptRoot 'common.ps1')
$versionInfo=Sync-ReleaseVersion
if (-not $CpuOnly -and (-not $AdobeSdk -or -not $NgxSdk -or -not $Runtime)) {
    $settings=Read-BuildSettings
    if (-not $AdobeSdk) { $AdobeSdk=$settings.AdobeSdk }
    if (-not $NgxSdk) { $NgxSdk=$settings.NgxSdk }
    if (-not $Runtime) { $Runtime=$settings.Runtime }
}
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$buildDir = Join-Path $projectRoot 'build'
if ($buildDir.Length -gt 130) { throw 'This folder path is too long for MSVC tracking files. Extract/clone the project into a shorter path such as C:\Dev\4x4Tools-DLSS5, then run setup-dev.ps1 again.' }
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio C++ Build Tools are required.' }
$vs = @(& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json | ConvertFrom-Json)[0]
if (-not $vs) { throw 'Visual Studio C++ Build Tools were not found.' }
$generators = @{18='Visual Studio 18 2026';17='Visual Studio 17 2022'}
$major = [int]($vs.installationVersion.Split('.')[0])
if (-not $generators.ContainsKey($major)) { throw 'Visual Studio 2022 or 2026 is required.' }
$cmake = Join-Path $vs.installationPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (-not (Test-Path -LiteralPath $cmake)) { $cmake = (Get-Command cmake -ErrorAction Stop).Source }
$ctest = Join-Path (Split-Path -Parent $cmake) 'ctest.exe'
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
function Run([string]$Executable, [string[]]$Arguments, [string]$Log) {
    $start = New-Object Diagnostics.ProcessStartInfo
    $start.FileName=$Executable; $start.WorkingDirectory=$projectRoot
    $start.UseShellExecute=$false; $start.CreateNoWindow=$true
    $start.RedirectStandardOutput=$true; $start.RedirectStandardError=$true
    $start.Arguments=($Arguments | ForEach-Object {
        if ($_ -match '"' -or $_.EndsWith('\')) { throw 'Unexpected quote or trailing backslash in build argument.' }
        '"'+$_+'"'
    }) -join ' '
    $taskPath=[Environment]::GetEnvironmentVariable('Path','Process')
    foreach ($key in @($start.EnvironmentVariables.Keys)) {
        if ([string]$key -ieq 'Path') { $start.EnvironmentVariables.Remove([string]$key) }
    }
    $start.EnvironmentVariables['Path']=$taskPath
    $start.EnvironmentVariables['LOCALAPPDATA']=Join-Path $buildDir 'test-data'
    $start.EnvironmentVariables['TOOLS_DISABLE_UPDATE_CHECK']='1'
    $process=New-Object Diagnostics.Process; $process.StartInfo=$start
    try {
        [void]$process.Start()
        $stdout=$process.StandardOutput.ReadToEndAsync(); $stderr=$process.StandardError.ReadToEndAsync()
        $process.WaitForExit(); $result=$stdout.Result+$stderr.Result
        $result | Set-Content -LiteralPath (Join-Path $buildDir $Log) -Encoding UTF8
        Write-Host $result
        if ($process.ExitCode -ne 0) { throw "Build step failed. See build/$Log." }
    } finally { $process.Dispose() }
}
$config=@('-S',$projectRoot,'-B',$buildDir,'-G',$generators[$major],'-A','x64')
$config+=('-DBUILD_PHOTOSHOP_PLUGIN='+$(if($Photoshop -and -not $CpuOnly){'ON'}else{'OFF'}))
if ($CpuOnly) { $config+='-DBUILD_ADOBE_PLUGIN=OFF' }
else {
    foreach ($dependency in @($AdobeSdk,$NgxSdk,$Runtime)) {
        if (-not $dependency -or -not (Test-Path -LiteralPath $dependency)) { throw 'Supply valid AdobeSdk, NgxSdk and Runtime paths. See docs/BUILDING.md.' }
    }
    $config+=@('-DBUILD_ADOBE_PLUGIN=ON',"-DAE_SDK_ROOT=$AdobeSdk","-DNGX_SDK_ROOT=$NgxSdk","-DNR_RUNTIME_FILE=$Runtime")
    if ($Photoshop) {
        $settings=Read-BuildSettings
        $config+=@("-DPHOTOSHOP_SDK_ROOT=$($settings.PhotoshopSdk)","-DLCMS_ROOT=$($settings.Lcms)")
    }
}
Run $cmake $config 'configure.log'
Run $cmake @('--build',$buildDir,'--config','Release','--parallel') 'build.log'
$testArgs=@('--test-dir',$buildDir,'-C','Release','--output-on-failure','--no-tests=error')
if ($HostedCI) { $testArgs+=@('-E','adobe_neural_hardware|photoshop_neural_hardware|installer_gpu_preflight') }
Run $ctest $testArgs 'tests.log'
Write-Host 'Build and tests passed.'
$binaries=@()
if (-not $CpuOnly) {
    $names=@('4x4Tools-DLSS5.aex','SupportCheck.exe','UpdateCheck.exe','runtime/nvngx_dlssnr.dll')
    if ($Photoshop) { $names+='4x4Tools-DLSS5-Photoshop.8bf' }
    $binaries=@($names | ForEach-Object { @{path=$_;sha256=(Get-Sha (Join-Path (Join-Path $buildDir 'Release') $_))} })
}
Write-JsonFile @{version=$versionInfo.Config.version;cpuOnly=[bool]$CpuOnly;hardwareValidated=(-not $HostedCI -and -not $CpuOnly);coreFingerprint=(Get-SourceFingerprint -CodeOnly);binaries=$binaries;created=(Get-Date -Format o)} (Join-Path $buildDir 'build-receipt.json')
