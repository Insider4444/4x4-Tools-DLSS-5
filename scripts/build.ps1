#Requires -Version 5.1
[CmdletBinding()]
param(
    [string]$AdobeSdk = $env:AE_SDK_BASE_PATH,
    [string]$NgxSdk = $env:DLSS_SDK_ROOT,
    [string]$Runtime = $env:DLSSNR_RUNTIME_DLL,
    [switch]$CpuOnly
)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$buildDir = Join-Path $projectRoot 'build'
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
if ($CpuOnly) { $config+='-DBUILD_ADOBE_PLUGIN=OFF' }
else {
    foreach ($dependency in @($AdobeSdk,$NgxSdk,$Runtime)) {
        if (-not $dependency -or -not (Test-Path -LiteralPath $dependency)) { throw 'Supply valid AdobeSdk, NgxSdk and Runtime paths. See docs/BUILDING.md.' }
    }
    $config+=@('-DBUILD_ADOBE_PLUGIN=ON',"-DAE_SDK_ROOT=$AdobeSdk","-DNGX_SDK_ROOT=$NgxSdk","-DNR_RUNTIME_FILE=$Runtime")
}
Run $cmake $config 'configure.log'
Run $cmake @('--build',$buildDir,'--config','Release','--parallel') 'build.log'
Run $ctest @('--test-dir',$buildDir,'-C','Release','--output-on-failure','--no-tests=error') 'tests.log'
Write-Host 'Build and tests passed.'
