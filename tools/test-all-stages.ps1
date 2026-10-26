param(
    [int[]]$Stages = 0..10,
    [int]$Include60Fps = 1,
    [int]$StopAfter = 300,
    [string]$OutputDir = ""
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $OutputDir) {
    $timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $OutputDir = Join-Path $projectRoot "artifacts\stage-benchmarks-$timestamp"
}

if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}

$exe = Join-Path $projectRoot "build\windows-64-pal-aurora\sms.exe"
if (-not (Test-Path $exe)) {
    throw "Executable not found: $exe"
}

$stageNames = @{
    0  = "Airport"
    1  = "Plaza"
    2  = "Bianco"
    3  = "Ricco"
    4  = "Gelato"
    5  = "Pinna"
    6  = "Sirena"
    7  = "Hotel"
    8  = "Noki"
    9  = "Pianta"
    10 = "Corona"
}

function Get-Median {
    param([double[]]$Values)
    if (-not $Values -or $Values.Count -eq 0) { return 0.0 }
    $sorted = $Values | Sort-Object
    $mid = [int]($sorted.Count / 2)
    if ($sorted.Count % 2 -eq 1) {
        return [math]::Round($sorted[$mid], 2)
    } else {
        return [math]::Round(($sorted[$mid - 1] + $sorted[$mid]) / 2.0, 2)
    }
}

$statsRegex = [regex]'\[gx-aurora\]\s+t=\d+s\s+[^:]+:\s+frame median\s+([\d.]+)\s+ms,\s+p90\s+([\d.]+)\s+ms,\s+worst\s+([\d.]+)\s+ms,\s+(\d+)\s+hitches;\s+per frame:\s+VI idle\s+([\d.]+)\s+ms,\s+Aurora submit\s+([\d.]+)\s+ms,\s+present\s+([\d.]+)\s+ms,\s+acquire\s+([\d.]+)\s+ms\s+\(\d+\s+over\s+4\s+ms\),\s+([\d.]+)\s+draws,\s+([\d.]+)\s+vertices,\s+([\d.]+)\s+texture loads,\s+([\d.]+)\s+rehashes,\s+([\d.]+)\s+copies'

function Run-StageTest {
    param(
        [int]$Stage,
        [string]$StageName,
        [string]$Warp,
        [int]$FrameRate = 30,
        [string]$RunLabel
    )

    Write-Host "=================================================="
    Write-Host "Testing Stage $Stage ($StageName) [$RunLabel]..."
    Write-Host "=================================================="

    $stageDir = Join-Path $OutputDir $RunLabel
    New-Item -ItemType Directory -Path $stageDir -Force | Out-Null
    $stdoutLog = Join-Path $stageDir "stdout.log"
    $stderrLog = Join-Path $stageDir "stderr.log"

    $env:PATH = "C:\msys64\mingw64\bin;$env:PATH"
    $env:SMS_SETTINGS = Join-Path $projectRoot "settings.txt"
    $env:SMS_BINDINGS = Join-Path $projectRoot "bindings.txt"
    $env:SMS_DISC_IMAGE = $null
    $env:SMS_DISC_ROOT = Join-Path $projectRoot "rom\GMSP01-extracted\files"
    $env:SMS_SAVE_DIR = Join-Path $projectRoot "saves\card-a"
    $env:SMS_FRONTEND_DIR = Join-Path $projectRoot "saves\frontend"

    $env:SMS_WARP = $Warp
    $env:SMS_AUTOPRESS = "START@30,START@60,A@90,A@120,A@150,A@180,A@210,A@240"
    $env:SMS_SKIP_MOVIES = "1"
    $env:SMS_GX_STOP_AFTER = "$StopAfter"
    $env:SMS_GX_STATS = "1"
    $env:SMS_AURORA_BACKEND = "d3d12"
    if ($FrameRate -eq 60) {
        $env:SMS_FRAME_RATE = "60"
    } else {
        Remove-Item env:SMS_FRAME_RATE -ErrorAction SilentlyContinue
    }

    $startTime = Get-Date
    $p = Start-Process -FilePath $exe -ArgumentList "--no-launcher" -WorkingDirectory $projectRoot -WindowStyle Normal -PassThru -RedirectStandardError $stderrLog -RedirectStandardOutput $stdoutLog
    $nativeHandle = $p.Handle
    $exited = $p.WaitForExit(45000)
    $exitCode = -1
    $cleanExit = $false
    if ($exited) {
        $p.WaitForExit()
        $exitCode = $p.ExitCode
        $cleanExit = ($exitCode -eq 0)
    } else {
        Write-Warning "Process timed out after 45 seconds! Killing process..."
        try { $p.Kill() } catch {}
    }
    $duration = ((Get-Date) - $startTime).TotalSeconds

    $stderrLines = if (Test-Path $stderrLog) { Get-Content $stderrLog } else { @() }
    
    # Parse stats
    $frames = @()
    foreach ($line in $stderrLines) {
        $m = $statsRegex.Match($line)
        if ($m.Success) {
            $frames += [PSCustomObject]@{
                FrameMedian = [double]$m.Groups[1].Value
                P90         = [double]$m.Groups[2].Value
                Worst       = [double]$m.Groups[3].Value
                Hitches     = [int]$m.Groups[4].Value
                VIIdle      = [double]$m.Groups[5].Value
                Submit      = [double]$m.Groups[6].Value
                Present     = [double]$m.Groups[7].Value
                Acquire     = [double]$m.Groups[8].Value
                Draws       = [double]$m.Groups[9].Value
                Vertices    = [double]$m.Groups[10].Value
                TexLoads    = [double]$m.Groups[11].Value
                Rehashes    = [double]$m.Groups[12].Value
                Copies      = [double]$m.Groups[13].Value
            }
        }
    }

    # Find warnings/errors (excluding standard RmlUi assert / font URL messages if any)
    $warnings = @()
    $errors = @()
    foreach ($line in $stderrLines) {
        $lower = $line.ToLower()
        if ($lower -match 'error|fatal' -and $lower -notmatch 'rmlui') {
            $errors += $line
        } elseif ($lower -match 'warning' -and $lower -notmatch 'rmlui') {
            $warnings += $line
        }
    }

    # Steady-state: in-stage frames where stage geometry is rendered (draws > 100)
    $steadyFrames = $frames | Where-Object { $_.Draws -gt 100 }
    if ($steadyFrames.Count -eq 0) {
        $steadyFrames = $frames
    }

    $steadyMedFrameTime = Get-Median ($steadyFrames | ForEach-Object { $_.FrameMedian })
    $steadyMedSubmit = Get-Median ($steadyFrames | ForEach-Object { $_.Submit })
    $steadyMedPresent = Get-Median ($steadyFrames | ForEach-Object { $_.Present })
    $steadyMedIdle = Get-Median ($steadyFrames | ForEach-Object { $_.VIIdle })
    $steadyMedDraws = Get-Median ($steadyFrames | ForEach-Object { $_.Draws })
    $steadyMedVerts = Get-Median ($steadyFrames | ForEach-Object { $_.Vertices })
    $steadyMedTexLoads = Get-Median ($steadyFrames | ForEach-Object { $_.TexLoads })
    $steadyMedCopies = Get-Median ($steadyFrames | ForEach-Object { $_.Copies })

    $allMedFrameTime = Get-Median ($frames | ForEach-Object { $_.FrameMedian })
    $allMedSubmit = Get-Median ($frames | ForEach-Object { $_.Submit })
    $allMedPresent = Get-Median ($frames | ForEach-Object { $_.Present })

    # Headroom calculation:
    # Target frame interval: 33.33ms at 30 fps, 16.67ms at 60 fps.
    # VI idle time represents the unused frame budget before vertical retrace deadline.
    $targetInterval = if ($FrameRate -eq 60) { 16.67 } else { 33.33 }
    $headroomMs = $steadyMedIdle
    $headroomPct = [math]::Round(($headroomMs / $targetInterval) * 100, 1)

    $result = [PSCustomObject]@{
        Stage            = $Stage
        StageName        = $StageName
        RunLabel         = $RunLabel
        Warp             = $Warp
        FrameRate        = $FrameRate
        ExitCode         = $exitCode
        CleanExit        = $cleanExit
        DurationSeconds  = [math]::Round($duration, 1)
        TotalFrames      = $frames.Count
        SteadyFrames     = $steadyFrames.Count
        MedianFrameTime  = $steadyMedFrameTime
        AuroraSubmitTime = $steadyMedSubmit
        PresentTime      = $steadyMedPresent
        VIIdleTime       = $steadyMedIdle
        HeadroomMs       = $headroomMs
        HeadroomPct      = $headroomPct
        DrawCount        = $steadyMedDraws
        VertexCount      = $steadyMedVerts
        TexLoadCount     = $steadyMedTexLoads
        CopiesCount      = $steadyMedCopies
        AllFramesMedian  = $allMedFrameTime
        AllFramesSubmit  = $allMedSubmit
        AllFramesPresent = $allMedPresent
        WarningCount     = $warnings.Count
        ErrorCount       = $errors.Count
        Warnings         = ($warnings -join "; ")
        Errors           = ($errors -join "; ")
    }

    $result | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $stageDir "result.json")
    Write-Host "Result: CleanExit=$cleanExit, FrameTime=${steadyMedFrameTime}ms, Submit=${steadyMedSubmit}ms, Present=${steadyMedPresent}ms, Draws=$steadyMedDraws, Verts=$steadyMedVerts, Copies=$steadyMedCopies"
    return $result
}

$results = @()

foreach ($stage in $Stages) {
    $name = if ($stageNames.ContainsKey($stage)) { $stageNames[$stage] } else { "Stage_$stage" }
    $warp = if ($stage -eq 1) { "1,0,1" } else { "$stage,0" }
    $label = "stage_$stage"
    $r = Run-StageTest -Stage $stage -StageName $name -Warp $warp -FrameRate 30 -RunLabel $label
    $results += $r
}

if ($Include60Fps) {
    $r60 = Run-StageTest -Stage 1 -StageName "Plaza" -Warp "1,0,1" -FrameRate 60 -RunLabel "stage_1_fps60"
    $results += $r60
}

# Save all results to JSON
$results | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $OutputDir "benchmark-results.json")

# Generate Markdown Table
$md = @"
# Super Mario Sunshine PC Port - Stage Benchmark & Profiling Report (Aurora / D3D12)

**Test Configuration:**
- Backend: Direct3D 12 (Aurora WebGPU)
- Autopress: `START@30,START@60,A@90,A@120,A@150,A@180,A@210,A@240`
- Skip Movies: 1
- Stop After: $StopAfter copies
- Stats Reporting: SMS_GX_STATS=1 (every frame)
- Timestamp: $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")
- Output Directory: $OutputDir

## Stage Benchmark Results Table

| Stage ID | Stage Name | Warp | FPS Mode | Clean Exit | Median Frame (ms) | Aurora Submit (ms) | Present (ms) | VI Idle / Headroom (ms) | Draws | Vertices | Tex Loads | Copies | Warnings/Errors |
|:---:|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---|
"@

foreach ($r in $results) {
    $status = if ($r.CleanExit) { "SUCCESS (0)" } else { "FAILED ($($r.ExitCode))" }
    $fpsStr = if ($r.FrameRate -eq 60) { "60 FPS" } else { "30 FPS" }
    $warnStr = if ($r.ErrorCount -gt 0) { "ERROR: $($r.ErrorCount)" } elseif ($r.WarningCount -gt 0) { "Warning ($($r.WarningCount))" } else { "None" }
    $warpCode = '`' + $r.Warp + '`'
    $md += "`n| $($r.Stage) | $($r.StageName) | $warpCode | $fpsStr | $status | $($r.MedianFrameTime) | $($r.AuroraSubmitTime) | $($r.PresentTime) | $($r.HeadroomMs) ($($r.HeadroomPct)%) | $($r.DrawCount) | $($r.VertexCount) | $($r.TexLoadCount) | $($r.CopiesCount) | $warnStr |"
}

$md += "`n`n## Summary & Key Observations`n"
Set-Content -Path (Join-Path $OutputDir "BENCHMARK_REPORT.md") -Value $md

Write-Host "`n"
Write-Host $md
Write-Host "`nAll benchmark runs completed! Results saved to $OutputDir"
