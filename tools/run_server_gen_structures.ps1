# run_server_gen_structures.ps1 — ground-truth chunks WITH structures, from the REAL server.
# Variant of run_server_gen.ps1 that flips generate-structures=true and generates into a SEPARATE
# world dir (world_structures) so it never clobbers the terrain-only `world/` that full_chunk uses.
# Forceloads a wide rectangle (structures are sparse) so several StructureStarts (villages / pillager
# outpost / etc.) land in the .mca; the createStructures gate then extracts the per-chunk
# `structures.starts` (structure id + bounding box + Children piece list) as ground truth.
#
#   pwsh mcpp/tools/run_server_gen_structures.ps1 [-FromChunkX -20 -FromChunkZ -20 -ToChunkX 20 -ToChunkZ 20 -GenSeconds 150]
param(
    [int]$FromChunkX = -20, [int]$FromChunkZ = -20,
    [int]$ToChunkX   = 20,  [int]$ToChunkZ   = 20,
    [int]$GenSeconds = 150
)
$ErrorActionPreference = 'Stop'
function Resolve-RepoRoot {
    $direct = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
    if (Test-Path (Join-Path $direct 'CMakeLists.txt')) { return $direct }
    return (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
}

$repo = Resolve-RepoRoot
$java = Get-ChildItem (Join-Path $repo '26.1.2\jdk25') -Recurse -Filter java.exe | Select-Object -First 1 -ExpandProperty FullName
$run  = Join-Path $repo '26.1.2\server_run'
$props = Join-Path $run 'server.properties'

# Back up server.properties, write a structures-enabled variant into a separate world.
$backup = Get-Content $props -Raw
try {
    Remove-Item (Join-Path $run 'world_structures') -Recurse -Force -ErrorAction SilentlyContinue
    $lines = Get-Content $props
    $seen = @{}
    $out = foreach ($l in $lines) {
        if ($l -match '^\s*generate-structures\s*=') { $seen['gs'] = $true; 'generate-structures=true' }
        elseif ($l -match '^\s*level-name\s*=')      { $seen['ln'] = $true; 'level-name=world_structures' }
        else { $l }
    }
    if (-not $seen['gs']) { $out += 'generate-structures=true' }
    if (-not $seen['ln']) { $out += 'level-name=world_structures' }
    Set-Content -Path $props -Value $out -Encoding ascii

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $java
    $psi.Arguments = '-Xmx3G -jar ..\server.jar nogui'
    $psi.WorkingDirectory = $run
    $psi.RedirectStandardInput = $true
    $psi.UseShellExecute = $false
    $p = [System.Diagnostics.Process]::Start($psi)

    function Send-Cmd([string]$cmd) {
        $bytes = [System.Text.Encoding]::ASCII.GetBytes($cmd + "`n")
        $p.StandardInput.BaseStream.Write($bytes, 0, $bytes.Length)
        $p.StandardInput.BaseStream.Flush()
        Write-Output "  > $cmd"
    }

    Start-Sleep -Seconds 12
    $nl = [byte[]](13, 10)
    $p.StandardInput.BaseStream.Write($nl, 0, 2); $p.StandardInput.BaseStream.Flush()
    Start-Sleep -Seconds 1
    Send-Cmd 'tick freeze'
    Start-Sleep -Seconds 1
    $x1 = $FromChunkX * 16; $z1 = $FromChunkZ * 16; $x2 = $ToChunkX * 16; $z2 = $ToChunkZ * 16
    Send-Cmd "forceload add $x1 $z1 $x2 $z2"
    Start-Sleep -Seconds $GenSeconds
    Send-Cmd 'save-all flush'
    Start-Sleep -Seconds 20
    Send-Cmd 'stop'
    if (-not $p.WaitForExit(90000)) { $p.Kill(); Write-Output 'killed after timeout' }
} finally {
    Set-Content -Path $props -Value $backup -Encoding ascii -NoNewline  # restore the terrain-only config
}

Write-Output "==== world_structures overworld region files ===="
Get-ChildItem (Join-Path $run 'world_structures\dimensions\minecraft\overworld\region') -ErrorAction SilentlyContinue | Select-Object Name,Length
