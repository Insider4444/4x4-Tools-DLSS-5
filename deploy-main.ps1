#Requires -Version 5.1
[CmdletBinding()]
param([string]$Version,[string]$NotesFile,[switch]$BuildOnly,[switch]$Wait)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'scripts\common.ps1')
$config=Read-ReleaseConfig $Version; [void](Sync-ReleaseVersion)
if ($NotesFile) {
    $notes=Get-Content -LiteralPath (Get-LocalPath $NotesFile) -Raw -Encoding UTF8
    [IO.File]::WriteAllText((Join-Path $PSScriptRoot 'update-note.md'),$notes,[Text.UTF8Encoding]::new($false))
    $config.notesFile='update-note.md'; Write-JsonFile $config (Join-Path $PSScriptRoot 'release-config.json')
}
$notesPath=Get-LocalPath $config.notesFile
if (-not $notesPath.StartsWith($PSScriptRoot+'\',[StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetExtension($notesPath) -ne '.md') { throw 'Set notesFile to a Markdown file inside this repository.' }
$notes=Get-Content -LiteralPath $notesPath -Raw -Encoding UTF8
if ($notes.Trim().Length -lt 20 -or $notes.Length -gt 100000) { throw 'Write release notes between 20 and 100000 characters.' }
$fingerprint=Get-SourceFingerprint -CodeOnly
if (-not $BuildOnly) {
    $receiptPath=Join-Path $PSScriptRoot '.local\local-install-receipt.json'
    if (-not (Test-Path -LiteralPath $receiptPath)) { throw 'Run debug-install.ps1 and test your footage before publishing. Use -BuildOnly to request an unpublished GitHub build.' }
    $receipt=Get-Content $receiptPath -Raw | ConvertFrom-Json
    if ($receipt.coreFingerprint -ne $fingerprint) { throw 'Code changed since local installation. Run debug-install.ps1 and test in Adobe first.' }
}
[void](Gh @('auth','status'))
if ((Git @('branch','--show-current')).Text.Trim() -ne 'main') { throw 'Integrate your code into main before deployment.' }
$allowedOrigins=@(('https://github.com/'+$config.repository+'.git'),('https://github.com/'+$config.repository),('git@github.com:'+$config.repository+'.git'))
if ((Git @('remote','get-url','origin')).Text.Trim().TrimEnd('/') -notin $allowedOrigins) { throw 'origin does not match release-config.json.' }
[void](Git @('fetch','origin','main','--tags'))
if ((Git @('merge-base','--is-ancestor','origin/main','HEAD') -AllowFailure).Code -ne 0) { throw 'Integrate newer GitHub main changes, then retest. No history will be overwritten.' }
$latest=Gh @('api',('repos/'+$config.repository+'/releases/latest')) -AllowFailure
if ($latest.Code -eq 0) {
    $release=$latest.Text | ConvertFrom-Json
    if (-not $BuildOnly -and [version]$config.version -le [version]$release.tag_name.TrimStart('v')) { throw ('Choose a version newer than '+$release.tag_name+'.') }
    Write-Host ('Changes since '+$release.tag_name+":`n"+(Git @('diff','--stat',$release.tag_name,'--','.')).Text)
} elseif ($latest.Error -notmatch '404') { throw $latest.Error }
if ((Git @('diff','--name-only','--diff-filter=U')).Text.Trim()) { throw 'Resolve Git conflicts first.' }
[void](Get-PublicFiles)
Write-Host ('Version: '+$config.version+"`nRelease notes:`n"+$notes)
if ((Git @('var','GIT_AUTHOR_IDENT') -AllowFailure).Code -ne 0) {
    $account=(Gh @('api','user')).Text | ConvertFrom-Json
    [void](Git @('config','user.name',$account.login))
    [void](Git @('config','user.email',($account.id.ToString()+'+'+$account.login+'@users.noreply.github.com')))
}
[void](Git @('add','-A','--','.'))
$diff=Git @('diff','--cached','--quiet') -AllowFailure
if ($diff.Code -eq 1) { [void](Git @('commit','-m',('Prepare 4x4Tools-DLSS5-win v'+$config.version))) }
elseif ($diff.Code -ne 0) { throw $diff.Error }
$head=(Git @('rev-parse','HEAD')).Text.Trim()
[void](Git @('push','origin','HEAD:main'))
$published=if ($BuildOnly) { 'false' } else { 'true' }
$before=[DateTime]::UtcNow.AddSeconds(-2)
[void](Gh @('workflow','run','release.yml','--repo',$config.repository,'--ref','main','-f',('version='+$config.version),'-f',('expected_sha='+$head),'-f',('publish='+$published)))
Write-Host ('GitHub is building commit '+$head+'. No local EXE or ZIP was uploaded.')
$run=$null
for ($attempt=0;$attempt -lt 12 -and -not $run;$attempt++) {
    $runs=(Gh @('run','list','--repo',$config.repository,'--workflow','release.yml','--event','workflow_dispatch','--commit',$head,'--limit','10','--json','databaseId,createdAt,url')).Text | ConvertFrom-Json
    $run=$runs | Where-Object { [DateTime]$_.createdAt -ge $before } | Select-Object -First 1
    if (-not $run) { Start-Sleep -Seconds 2 }
}
if (-not $run) { throw ('Dispatch accepted; locate the run at https://github.com/'+$config.repository+'/actions/workflows/release.yml before retrying.') }
Write-Host $run.url
Write-JsonFile @{runId=$run.databaseId;url=$run.url;commit=$head;version=$config.version;publish=(-not $BuildOnly)} (Join-Path $PSScriptRoot '.local\last-deployment.json')
if ($Wait) {
    $watched=Invoke-Native (Find-Tool 'gh') @('run','watch',([string]$run.databaseId),'--repo',$config.repository,'--exit-status') -TimeoutSeconds 2400
    Write-Host $watched.Text
}
