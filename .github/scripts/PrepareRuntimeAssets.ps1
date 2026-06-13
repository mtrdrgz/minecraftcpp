$ErrorActionPreference = 'Stop'

$versionId = '26.1.2'
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$assetsRoot = Join-Path $repoRoot 'assets'
$indexesDir = Join-Path $assetsRoot 'indexes'
$objectsDir = Join-Path $assetsRoot 'objects'

New-Item -ItemType Directory -Force -Path $indexesDir | Out-Null
New-Item -ItemType Directory -Force -Path $objectsDir | Out-Null

$metaBase = 'https://' + 'piston-meta.mojang.com'
$resBase = 'https://' + 'resources.download.minecraft.net'
$manifestUrl = $metaBase + '/mc/game/version_manifest_v2.json'

$manifest = Invoke-RestMethod -Uri $manifestUrl -UseBasicParsing
$version = $manifest.versions | Where-Object { $_.id -eq $versionId } | Select-Object -First 1
if (-not $version) { throw "Version not found: $versionId" }

$versionJson = Invoke-RestMethod -Uri $version.url -UseBasicParsing
$assetIndex = Invoke-RestMethod -Uri $versionJson.assetIndex.url -UseBasicParsing
$indexPath = Join-Path $indexesDir ("$($versionJson.assetIndex.id).json")
($assetIndex | ConvertTo-Json -Depth 100) | Set-Content -Encoding UTF8 $indexPath

$needed = @()
foreach ($prop in $assetIndex.objects.PSObject.Properties) {
    $name = [string]$prop.Name
    if ($name.StartsWith('minecraft/textures/block/') -and $name.EndsWith('.png')) { $needed += $name; continue }
    if ($name.StartsWith('minecraft/textures/gui/title/background/') -and $name.EndsWith('.png')) { $needed += $name; continue }
    if ($name.StartsWith('minecraft/textures/gui/sprites/hud/') -and $name.EndsWith('.png')) { $needed += $name; continue }
}

Write-Host "Downloading $($needed.Count) runtime assets"
foreach ($name in $needed) {
    $info = $assetIndex.objects.$name
    $hash = [string]$info.hash
    $prefix = $hash.Substring(0, 2)
    $dir = Join-Path $objectsDir $prefix
    $dst = Join-Path $dir $hash
    if (Test-Path $dst) { continue }
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    Invoke-WebRequest -Uri ($resBase + '/' + $prefix + '/' + $hash) -OutFile $dst -UseBasicParsing
}

Write-Host "Prepared runtime assets in $assetsRoot"
