# run_server_gen.ps1 — generate ground-truth chunks with the REAL Minecraft server.
# Runs server.jar (generate-structures=false; see 26.1.2/server_run/server.properties),
# forceloads a chunk rectangle around origin so those chunks generate to FULL status
# (terrain + carvers + features, NO structures), flushes, and stops cleanly.
#
# stdin MUST be written as raw ASCII bytes to BaseStream — PowerShell's StreamWriter
# (WriteLine / `echo |`) prepends a UTF-8 BOM that the server parses as part of the
# command ("﻿stop<--[HERE]"), so the command is silently rejected.
#
#   pwsh mcpp/tools/run_server_gen.ps1 [-FromChunkX -1 -FromChunkZ -1 -ToChunkX 12 -ToChunkZ 12 -GenSeconds 55]
param(
    [int]$FromChunkX = -1, [int]$FromChunkZ = -1,
    [int]$ToChunkX   = 12, [int]$ToChunkZ   = 12,
    [int]$GenSeconds = 55
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$java = Get-ChildItem (Join-Path $repo '26.1.2\jdk25') -Recurse -Filter java.exe | Select-Object -First 1 -ExpandProperty FullName
$run  = Join-Path $repo '26.1.2\server_run'
Remove-Item (Join-Path $run 'world') -Recurse -Force -ErrorAction SilentlyContinue

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

Start-Sleep -Seconds 10                      # let startup reach "Done"
# Absorb any stream preamble/BOM onto a throwaway blank line so the first REAL command
# is not corrupted with a leading U+FEFF ("﻿forceload<--[HERE]" => unknown command).
$nl = [byte[]](13, 10)
$p.StandardInput.BaseStream.Write($nl, 0, 2)
$p.StandardInput.BaseStream.Flush()
Start-Sleep -Seconds 1
# Freeze gameplay ticking BEFORE forceloading so the generated chunks never tick: chunk
# generation still proceeds (it is driven by the chunk system, not the frozen game tick),
# but fluids do NOT flow, random ticks do not fire, etc. Without this the forced/spawn
# chunks tick for ~70s and lava/water flow into carved caves (and lava+water -> cobblestone),
# contaminating the .mca with runtime state that pure worldgen never produces.
Send-Cmd 'tick freeze'
Start-Sleep -Seconds 1
# forceload uses BLOCK coords; *16 converts chunk coords to the min block of each chunk.
$x1 = $FromChunkX * 16; $z1 = $FromChunkZ * 16; $x2 = $ToChunkX * 16; $z2 = $ToChunkZ * 16
Send-Cmd "forceload add $x1 $z1 $x2 $z2"
Start-Sleep -Seconds $GenSeconds             # let the forced chunks generate to full
Send-Cmd 'save-all flush'
Start-Sleep -Seconds 15
Send-Cmd 'stop'
if (-not $p.WaitForExit(60000)) { $p.Kill(); Write-Output 'killed after timeout' }

Write-Output "==== overworld region files ===="
Get-ChildItem (Join-Path $run 'world\dimensions\minecraft\overworld\region') -ErrorAction SilentlyContinue | Select-Object Name,Length
