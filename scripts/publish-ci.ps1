#Requires -Version 5.1
# Called only by the protected workflow_dispatch release job on main.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'common.ps1')
if ($env:GITHUB_ACTIONS -ne 'true' -or $env:GITHUB_REF -ne 'refs/heads/main') { throw 'Publishing runs only in the GitHub release workflow on main.' }
$config=Read-ReleaseConfig; $tag='v'+$config.version
$head=(Git @('rev-parse','HEAD')).Text.Trim()
if ($head -ne $env:GITHUB_SHA -or $config.version -ne $env:RELEASE_VERSION) { throw 'Workflow source/version mismatch.' }
$package=Get-Content (Join-Path $ProjectRoot 'build/package-result.json') -Raw | ConvertFrom-Json
$fingerprint=Get-SourceFingerprint -CodeOnly
if ($package.version -ne $config.version -or $package.coreFingerprint -ne $fingerprint) { throw 'Package does not match the committed source.' }
$preparedNotes=Get-LocalPath $config.notesFile
if (-not $preparedNotes.StartsWith($ProjectRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Notes must be inside the source repository.' }
$notes=Get-Content $preparedNotes -Raw -Encoding UTF8
if ($notes.Trim().Length -lt 20) { throw 'Release notes are missing.' }
$assets=@($package.exe,$package.zip,$package.checksums)
$latest=Gh @('api',('repos/'+$config.repository+'/releases/latest')) -AllowFailure
if ($latest.Code -eq 0 -and [version]$config.version -le [version](($latest.Text | ConvertFrom-Json).tag_name.TrimStart('v'))) { throw 'Choose a version newer than the latest published release.' }
if ($latest.Code -ne 0 -and $latest.Error -notmatch '404') { throw $latest.Error }
$existing=Gh @('release','view',$tag,'--repo',$config.repository,'--json','isDraft,targetCommitish,assets') -AllowFailure
if ($existing.Code -eq 0) {
    $draft=$existing.Text | ConvertFrom-Json
    if (-not $draft.isDraft -or $draft.targetCommitish -ne $head) { throw 'Existing release does not match this draft/commit. Review it on GitHub; this script will not replace a public release.' }
    [void](Gh @('release','edit',$tag,'--repo',$config.repository,'--title',($config.releaseName+' '+$tag),'--notes-file',$preparedNotes))
} else {
    if (($existing.Text+$existing.Error) -notmatch '(not found|404)') { throw ($existing.Text+$existing.Error) }
    $remoteTag=Git @('ls-remote','--tags','origin',('refs/tags/'+$tag))
    if ($remoteTag.Text.Trim()) { throw 'A tag already exists without this draft. Choose a new version or review the tag manually.' }
    [void](Gh @('release','create',$tag,'--repo',$config.repository,'--target',$head,'--draft','--title',($config.releaseName+' '+$tag),'--notes-file',$preparedNotes))
}
[void](Gh (@('release','upload',$tag,'--repo',$config.repository,'--clobber')+$assets))
# The tag endpoint only resolves published releases. Resolve this authenticated
# draft with gh first, then fetch its numeric REST URL for asset digests.
$draftUrl=(Gh @('release','view',$tag,'--repo',$config.repository,'--json','apiUrl','--jq','.apiUrl')).Text.Trim()
if ($draftUrl -notmatch ('^https://api\.github\.com/repos/'+[regex]::Escape($config.repository)+'/releases/[0-9]+$')) { throw 'Unexpected draft API URL.' }
$remote=(Gh @('api',$draftUrl)).Text | ConvertFrom-Json
if (-not $remote.draft -or @($remote.assets).Count -ne 3) { throw 'Draft must contain exactly the installer, manual ZIP and SHA256SUMS.txt. Review unexpected assets before publishing.' }
foreach ($asset in $assets) {
    $match=@($remote.assets | Where-Object { $_.name -ceq [IO.Path]::GetFileName($asset) })
    if ($match.Count -ne 1 -or $match[0].digest -ne ('sha256:'+(Get-Sha $asset))) { throw 'Uploaded asset checksum mismatch. The release remains a draft.' }
}
if ((Get-SourceFingerprint -CodeOnly) -ne $fingerprint -or (Git @('status','--porcelain')).Text.Trim()) { throw 'Source changed during upload. The release remains a draft; review and retest.' }
[void](Gh @('release','edit',$tag,'--repo',$config.repository,'--draft=false','--latest'))
Write-Host ('Published: https://github.com/'+$config.repository+'/releases/tag/'+$tag)
