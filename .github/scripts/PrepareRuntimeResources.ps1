$ErrorActionPreference = 'Stop'
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$srcAssetsDir = Join-Path $repoRoot 'mcpp\src\assets'
New-Item -ItemType Directory -Force -Path $srcAssetsDir | Out-Null
Write-Host 'Preparing transient runtime resources'
