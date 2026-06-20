# run_groundtruth.ps1 — compile + run a Java worldgen parity ground-truth
# generator against the REAL Minecraft 26.1.2 classes, emitting a TSV that a C++
# *_parity test compares against. This is the parity harness the 1:1 rebuild
# depends on (see mcpp/docs/WORLDGEN_1TO1_FOUNDATION.md §7).
#
# Requires the git-ignored real-Java runtime under 26.1.2/ (fetch per AGENTS.md):
#   * 26.1.2/client.jar           (the real bytecode; SHA1-verified)
#   * 26.1.2/libs/*.jar           (datafixerupper, guava, fastutil, ... from the
#                                  version manifest libraries[])
#   * 26.1.2/jdk25/**/bin/java(.exe), javac(.exe)  (JDK 25 — client.jar is v69)
#
# Usage (from repo root):
#   mcpp/tools/run_groundtruth.ps1 -Tool WorldgenRandomParity -Out mcpp/build/worldgen_random_cases.tsv
#   mcpp/tools/run_groundtruth.ps1 -Tool DensityParity -Out mcpp/build/density_cases.tsv -ToolArgs "0 1"
param(
    [Parameter(Mandatory = $true)][string]$Tool,                 # Java class name in mcpp/tools/<Tool>.java
    [Parameter(Mandatory = $true)][string]$Out,                  # TSV output path
    [string]$ToolArgs = ""                                       # args passed to the Java main()
)
$ErrorActionPreference = "Stop"

function Resolve-RepoRoot {
    $direct = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
    if (Test-Path (Join-Path $direct "CMakeLists.txt")) { return $direct }
    return (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
}

$repo = Resolve-RepoRoot
$jar  = Join-Path $repo "26.1.2\client.jar"
$libs = Join-Path $repo "26.1.2\libs"
$src  = Join-Path $PSScriptRoot "$Tool.java"
$classes = Join-Path $repo "26.1.2\parity_classes"

$javaBin = Get-ChildItem (Join-Path $repo "26.1.2\jdk25") -Recurse -Filter "java.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $javaBin) { throw "JDK 25 not found under 26.1.2/jdk25 (fetch per AGENTS.md)." }
$bin = Split-Path -Parent $javaBin.FullName
foreach ($p in @($jar, $libs, $src)) { if (-not (Test-Path $p)) { throw "Missing required path: $p" } }

New-Item -ItemType Directory -Force $classes | Out-Null
$cp = "$classes;$jar;$libs\*"

& "$bin\javac.exe" -cp "$jar;$libs\*" -d $classes $src
if ($LASTEXITCODE -ne 0) { throw "javac failed for $Tool" }

New-Item -ItemType Directory -Force (Split-Path -Parent $Out) | Out-Null
# JVM deprecation warnings (e.g. joml's sun.misc.Unsafe) print to stderr; route
# them to a log so ErrorActionPreference=Stop doesn't treat them as fatal, and
# write stdout as ASCII (PowerShell's '>' default is UTF-16, which a byte-reading
# C++ test would choke on).
$errLog = "$Out.stderr.log"
$ErrorActionPreference = 'Continue'
if ([string]::IsNullOrWhiteSpace($ToolArgs)) {
    & "$bin\java.exe" -cp $cp $Tool 2>$errLog | Out-File -FilePath $Out -Encoding ascii
} else {
    & "$bin\java.exe" -cp $cp $Tool ($ToolArgs -split ' ') 2>$errLog | Out-File -FilePath $Out -Encoding ascii
}
$code = $LASTEXITCODE
$ErrorActionPreference = 'Stop'
if ($code -ne 0) { Write-Host (Get-Content $errLog -ErrorAction SilentlyContinue | Select-Object -Last 15 | Out-String); throw "$Tool run failed (exit $code)" }
Write-Host ("Ground truth written: {0} ({1} lines)" -f $Out, (Get-Content $Out | Measure-Object -Line).Lines)
