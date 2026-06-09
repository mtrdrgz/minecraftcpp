# provision_parity_runtime.ps1 — fetch the real-Java runtime needed to run the
# 26.1.2 worldgen ground-truth (and the server-jar full-chunk byte-match harness):
#   26.1.2/server.jar  (sha1-verified against the version manifest)
#   26.1.2/libs/*.jar   (version-manifest libraries[] main artifacts)
#   26.1.2/jdk25/**     (JDK 25 — client/server jar is Java 25 bytecode, v69)
#
# Everything lands under the git-ignored 26.1.2/ dir; nothing proprietary is
# committed. Idempotent: skips anything already present/valid. Run from anywhere:
#   pwsh mcpp/tools/provision_parity_runtime.ps1 [-Force]
param([switch]$Force)
$ErrorActionPreference = 'Stop'
$repo  = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$mc    = Join-Path $repo '26.1.2'
$vjson = Join-Path $mc '26.1.2.json'
if (-not (Test-Path $vjson)) { throw "Missing version manifest: $vjson" }
$meta = Get-Content $vjson -Raw | ConvertFrom-Json

function Get-File($url, $dest) {
    Write-Host "GET $url"
    & curl.exe -L --fail --retry 3 --retry-delay 2 -o $dest $url
    if ($LASTEXITCODE -ne 0) { throw "download failed ($LASTEXITCODE): $url" }
}

# 1. server.jar (sha1-verified) ------------------------------------------------
$server    = Join-Path $mc 'server.jar'
$serverSha = $meta.downloads.server.sha1
$need = $true
if ((Test-Path $server) -and -not $Force) {
    $h = (Get-FileHash $server -Algorithm SHA1).Hash.ToLower()
    if ($h -eq $serverSha) { $need = $false; Write-Host "server.jar present (sha1 ok)" }
    else { Write-Host "server.jar sha1 mismatch ($h); re-downloading" }
}
if ($need) {
    Get-File $meta.downloads.server.url $server
    $h = (Get-FileHash $server -Algorithm SHA1).Hash.ToLower()
    if ($h -ne $serverSha) { throw "server.jar sha1 mismatch after download: $h != $serverSha" }
    Write-Host "server.jar OK (sha1 verified: $h)"
}

# 2. libraries (main artifacts only; skip natives/classifiers) -----------------
$libs = Join-Path $mc 'libs'
New-Item -ItemType Directory -Force $libs | Out-Null
$libCount = 0
foreach ($lib in $meta.libraries) {
    $art = $lib.downloads.artifact
    if (-not $art -or -not $art.url) { continue }
    $name = Split-Path $art.url -Leaf
    $dest = Join-Path $libs $name
    if ((Test-Path $dest) -and -not $Force) { $libCount++; continue }
    Get-File $art.url $dest
    $libCount++
}
Write-Host ("libs: {0} jars in {1}" -f $libCount, $libs)

# 3. JDK 25 (Adoptium; GA then EA fallback) ------------------------------------
$jdk = Join-Path $mc 'jdk25'
$haveJava = Get-ChildItem $jdk -Recurse -Filter java.exe -ErrorAction SilentlyContinue | Select-Object -First 1
if ((-not $haveJava) -or $Force) {
    $zip  = Join-Path $mc 'jdk25.zip'
    $urls = @(
        'https://api.adoptium.net/v3/binary/latest/25/ga/windows/x64/jdk/hotspot/normal/eclipse',
        'https://api.adoptium.net/v3/binary/latest/25/ea/windows/x64/jdk/hotspot/normal/eclipse'
    )
    $ok = $false
    foreach ($u in $urls) {
        try { Get-File $u $zip; $ok = $true; break } catch { Write-Host "fallback: $($_.Exception.Message)" }
    }
    if (-not $ok) { throw "JDK 25 download failed (GA + EA)" }
    if (Test-Path $jdk) { Remove-Item $jdk -Recurse -Force }
    New-Item -ItemType Directory -Force $jdk | Out-Null
    Expand-Archive -Path $zip -DestinationPath $jdk -Force
    Remove-Item $zip -Force
    $haveJava = Get-ChildItem $jdk -Recurse -Filter java.exe -ErrorAction SilentlyContinue | Select-Object -First 1
}
if (-not $haveJava) { throw "java.exe not found under $jdk after extraction" }
Write-Host "JDK 25 java.exe: $($haveJava.FullName)"
& $haveJava.FullName -version
Write-Host "PROVISION COMPLETE"
