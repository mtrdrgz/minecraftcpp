# run_server_groundtruth.ps1 - compile + run a Java ground-truth helper against
# the named server jar unpacked by the real Minecraft server runtime.
#
# Use this for helpers that only need server-side named classes, such as
# BlockStateRegistryParity and ServerChunkDump, when 26.1.2/client.jar is not
# present. Output is ASCII TSV, matching the C++ parity readers.
#
# Usage (from repo root):
#   mcpp/tools/run_server_groundtruth.ps1 -Tool BlockStateRegistryParity -Out mcpp/build/block_state_registry.tsv
#   mcpp/tools/run_server_groundtruth.ps1 -Tool ServerChunkDump -Out mcpp/build/server_chunk_cases.tsv -ToolArgs "1 -30,49"
param(
    [Parameter(Mandatory = $true)][string]$Tool,
    [Parameter(Mandatory = $true)][string]$Out,
    [string]$ToolArgs = ""
)
$ErrorActionPreference = "Stop"

function Resolve-RepoRoot {
    $direct = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
    if (Test-Path (Join-Path $direct "CMakeLists.txt")) { return $direct }
    return (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
}

$repo = Resolve-RepoRoot
$src = Join-Path $PSScriptRoot "$Tool.java"
$classes = Join-Path $repo "26.1.2\parity_classes_server"
$serverJar = Join-Path $repo "26.1.2\server_run\versions\26.1.2\server-26.1.2.jar"
$serverLibs = Join-Path $repo "26.1.2\server_run\libraries"

$javaBin = Get-ChildItem (Join-Path $repo "26.1.2\jdk25") -Recurse -Filter "java.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $javaBin) { throw "JDK 25 not found under 26.1.2/jdk25." }
$bin = Split-Path -Parent $javaBin.FullName
foreach ($p in @($src, $serverJar, $serverLibs)) { if (-not (Test-Path $p)) { throw "Missing required path: $p" } }

New-Item -ItemType Directory -Force $classes | Out-Null
$jars = (Get-ChildItem -Recurse -File $serverLibs -Filter "*.jar" | ForEach-Object FullName) -join ";"
$compileCp = "$serverJar;$jars"
$runCp = "$classes;$serverJar;$jars"

& (Join-Path $bin "javac.exe") -cp $compileCp -d $classes $src
if ($LASTEXITCODE -ne 0) { throw "javac failed for $Tool" }

New-Item -ItemType Directory -Force (Split-Path -Parent $Out) | Out-Null
$errLog = "$Out.stderr.log"
$ErrorActionPreference = "Continue"
if ([string]::IsNullOrWhiteSpace($ToolArgs)) {
    & (Join-Path $bin "java.exe") -cp $runCp $Tool 2>$errLog | Out-File -FilePath $Out -Encoding ascii
} else {
    & (Join-Path $bin "java.exe") -cp $runCp $Tool ($ToolArgs -split " ") 2>$errLog | Out-File -FilePath $Out -Encoding ascii
}
$code = $LASTEXITCODE
$ErrorActionPreference = "Stop"
if ($code -ne 0) {
    Write-Host (Get-Content $errLog -ErrorAction SilentlyContinue | Select-Object -Last 20 | Out-String)
    throw "$Tool run failed (exit $code)"
}
Write-Host ("Server ground truth written: {0} ({1} lines)" -f $Out, (Get-Content $Out | Measure-Object -Line).Lines)
