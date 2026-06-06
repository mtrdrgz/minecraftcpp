param(
    [Parameter(Mandatory = $true)]
    [string]$FilePath,

    [string]$Arguments = "",

    [int]$TimeoutSec = 900,

    [string]$LogPath = ""
)

$ErrorActionPreference = "Stop"

$processPath = [System.Environment]::GetEnvironmentVariable("Path", "Process")
if ([string]::IsNullOrEmpty($processPath)) {
    $processPath = [System.Environment]::GetEnvironmentVariable("PATH", "Process")
}
if (-not [string]::IsNullOrEmpty($processPath)) {
    [System.Environment]::SetEnvironmentVariable("PATH", $null, "Process")
    [System.Environment]::SetEnvironmentVariable("Path", $processPath, "Process")
}

if ([string]::IsNullOrWhiteSpace($LogPath)) {
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $LogPath = Join-Path (Get-Location) "build\logs\run-$stamp.log"
}

$logDir = Split-Path -Parent $LogPath
if ($logDir -and -not (Test-Path -LiteralPath $logDir)) {
    New-Item -ItemType Directory -Force -Path $logDir | Out-Null
}

$stdoutPath = "$LogPath.stdout"
$stderrPath = "$LogPath.stderr"
$exitPath = "$LogPath.exit"
$cmdScriptPath = "$LogPath.cmd"

function Write-Log([string]$Message) {
    "[$(Get-Date -Format o)] $Message" | Out-File -FilePath $LogPath -Encoding utf8 -Append
}

function Get-DescendantPids([int]$RootPid) {
    try {
        $all = Get-CimInstance Win32_Process |
            Select-Object ProcessId, ParentProcessId
    } catch {
        Write-Log "Get-CimInstance failed while collecting descendants: $($_.Exception.Message)"
        return @()
    }

    $childrenByParent = @{}
    foreach ($p in $all) {
        $parentId = [int]$p.ParentProcessId
        if (-not $childrenByParent.ContainsKey($parentId)) {
            $childrenByParent[$parentId] = New-Object System.Collections.Generic.List[int]
        }
        $childrenByParent[$parentId].Add([int]$p.ProcessId)
    }

    $result = New-Object System.Collections.Generic.List[int]
    $queue = New-Object System.Collections.Generic.Queue[int]
    $queue.Enqueue($RootPid)
    while ($queue.Count -gt 0) {
        $currentPid = $queue.Dequeue()
        if (-not $childrenByParent.ContainsKey($currentPid)) {
            continue
        }
        foreach ($child in $childrenByParent[$currentPid]) {
            $result.Add($child)
            $queue.Enqueue($child)
        }
    }
    return @($result)
}

function Stop-ProcessTree([int]$RootPid) {
    $descendants = @(Get-DescendantPids $RootPid)
    [Array]::Reverse($descendants)
    $targets = @($descendants + $RootPid) | Select-Object -Unique
    Write-Log "Stopping process tree: $($targets -join ', ')"

    foreach ($targetPid in $targets) {
        try {
            Stop-Process -Id $targetPid -Force -ErrorAction Stop
            Write-Log "Stop-Process succeeded for PID $targetPid"
        } catch {
            Write-Log "Stop-Process failed for PID ${targetPid}: $($_.Exception.Message)"
        }
    }

    Start-Sleep -Seconds 1
    $alive = @()
    foreach ($targetPid in $targets) {
        if (Get-Process -Id $targetPid -ErrorAction SilentlyContinue) {
            $alive += $targetPid
        }
    }

    foreach ($targetPid in $alive) {
        Write-Log "PID $targetPid still alive; running taskkill with 10s guard"
        $tkOut = "$LogPath.taskkill-$targetPid.out"
        $tkErr = "$LogPath.taskkill-$targetPid.err"
        $tk = Start-Process -FilePath "taskkill" `
            -ArgumentList "/PID $targetPid /T /F" `
            -NoNewWindow `
            -RedirectStandardOutput $tkOut `
            -RedirectStandardError $tkErr `
            -PassThru
        if (-not $tk.WaitForExit(10000)) {
            Write-Log "taskkill for PID $targetPid exceeded 10s; stopping taskkill PID $($tk.Id)"
            Stop-Process -Id $tk.Id -Force -ErrorAction SilentlyContinue
        }
        if (Test-Path -LiteralPath $tkOut) {
            Get-Content -LiteralPath $tkOut -Raw | Out-File -FilePath $LogPath -Encoding utf8 -Append
        }
        if (Test-Path -LiteralPath $tkErr) {
            Get-Content -LiteralPath $tkErr -Raw | Out-File -FilePath $LogPath -Encoding utf8 -Append
        }
    }
}

$commandInfo = Get-Command $FilePath -ErrorAction SilentlyContinue
if ($commandInfo -and $commandInfo.Source) {
    $runPath = $commandInfo.Source
} else {
    $runPath = $FilePath
}

@(
    "@echo off",
    "`"$runPath`" $Arguments > `"$stdoutPath`" 2> `"$stderrPath`"",
    "set EXITCODE=%ERRORLEVEL%",
    "echo %EXITCODE% > `"$exitPath`"",
    "exit /b %EXITCODE%"
) | Set-Content -LiteralPath $cmdScriptPath -Encoding ascii

"[$(Get-Date -Format o)] RUN $FilePath $Arguments" | Out-File -FilePath $LogPath -Encoding utf8
Write-Log "TIMEOUT ${TimeoutSec}s"

$process = Start-Process -FilePath $env:ComSpec `
    -ArgumentList @("/d", "/c", "`"$cmdScriptPath`"") `
    -NoNewWindow `
    -PassThru

$started = Get-Date
$lastHeartbeat = $started
while (-not $process.HasExited) {
    Start-Sleep -Seconds 2
    $process.Refresh()
    $elapsed = ((Get-Date) - $started).TotalSeconds
    if (((Get-Date) - $lastHeartbeat).TotalSeconds -ge 30) {
        Write-Log "still running after $([int]$elapsed)s (PID $($process.Id))"
        Write-Host "still running after $([int]$elapsed)s (PID $($process.Id)); log: $LogPath"
        $lastHeartbeat = Get-Date
    }
    if ($elapsed -ge $TimeoutSec) {
        Write-Log "TIMEOUT after $([int]$elapsed)s, killing PID $($process.Id)"
        Stop-ProcessTree $process.Id
        if (Test-Path -LiteralPath $stdoutPath) {
            Get-Content -LiteralPath $stdoutPath -Raw |
                Out-File -FilePath $LogPath -Encoding utf8 -Append
        }
        if (Test-Path -LiteralPath $stderrPath) {
            Get-Content -LiteralPath $stderrPath -Raw |
                Out-File -FilePath $LogPath -Encoding utf8 -Append
        }
        Write-Host "Command timed out after $([int]$elapsed)s. Log: $LogPath"
        exit 124
    }
}

$process.WaitForExit()
$exitCode = $process.ExitCode
if (Test-Path -LiteralPath $exitPath) {
    $rawExit = (Get-Content -LiteralPath $exitPath -Raw).Trim()
    $parsedExit = 0
    if ([int]::TryParse($rawExit, [ref]$parsedExit)) {
        $exitCode = $parsedExit
    }
}

if (Test-Path -LiteralPath $stdoutPath) {
    Get-Content -LiteralPath $stdoutPath -Raw |
        Out-File -FilePath $LogPath -Encoding utf8 -Append
}
if (Test-Path -LiteralPath $stderrPath) {
    Get-Content -LiteralPath $stderrPath -Raw |
        Out-File -FilePath $LogPath -Encoding utf8 -Append
}

Write-Log "EXIT $exitCode"

Write-Host "Command exited with code $exitCode. Log: $LogPath"
Write-Host "----- last log lines -----"
Get-Content -LiteralPath $LogPath -Tail 200
exit $exitCode
