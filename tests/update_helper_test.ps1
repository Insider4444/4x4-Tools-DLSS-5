#Requires -Version 5.1
param([Parameter(Mandatory=$true)][string]$Helper)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\scripts\common.ps1')
$original=$env:LOCALAPPDATA
$root=Join-Path $ProjectRoot ('artifacts\update-helper-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root -Force | Out-Null
function Assert([bool]$Condition,[string]$Message) { if (-not $Condition) { throw $Message } }
try {
    $env:LOCALAPPDATA=$root
    $fixture=Join-Path $root 'release.json'; $cache=Join-Path $root '4x4Tools\Updates\state.txt'
    [IO.File]::WriteAllText($fixture,'{"tag_name":"v1.10.0","draft":false,"prerelease":false}')
    [void](Invoke-Native $Helper @('--fixture',$fixture))
    Assert ((Get-Content $cache -Raw) -match '1.10.0') 'Stable update was not cached.'
    foreach ($json in @('{"tag_name":"v99.0.0","draft":true,"prerelease":false}','{"tag_name":"v99.0.0","draft":false,"prerelease":true}','{"tag_name":"v99.0.0","draft":false,"prerelease":false,"bad":1e}','invalid')) {
        [IO.File]::WriteAllText($fixture,$json)
        Assert ((Invoke-Native $Helper @('--fixture',$fixture) -AllowFailure).Code -eq 3) 'Invalid/draft release was accepted.'
        Assert ((Get-Content $cache -Raw) -match '1.10.0') 'Failed request lost a previously valid update.'
    }
    $before=Get-Sha $cache
    [void](Invoke-Native $Helper @()) # Fresh daily cache: must return without network or a cache rewrite.
    Assert ((Get-Sha $cache) -eq $before) 'Daily cache was bypassed.'
    [IO.File]::WriteAllText((Join-Path $root '4x4Tools\Updates\disabled'),'disabled')
    [IO.File]::WriteAllText($cache,"4x4Tools-update-v1`n1`n1.10.0`n")
    $before=Get-Sha $cache
    [void](Invoke-Native $Helper @())
    Assert ((Get-Sha $cache) -eq $before) 'Disabled check changed an expired cache.'
    Write-Host 'PASS update helper stable filtering, error preservation, 24-hour cache and disabled checks'
} finally { $env:LOCALAPPDATA=$original }
