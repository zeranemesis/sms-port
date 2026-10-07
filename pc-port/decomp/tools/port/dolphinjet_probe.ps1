<#
.SYNOPSIS
    Sonde de crash + performance pour dolphinjet.exe (route Aurora, Windows uniquement).

.DESCRIPTION
    Contrairement a ModernGekko, DolphinJet n'a pas de protocole d'automatisation
    integre (pas de --automation-dir, pas de status.txt avec fps/vps/speed reels).
    Cette sonde mesure donc ce qui est reellement mesurable de l'exterieur : l'etat
    du process (vivant / plante, avec code de sortie et fin de stderr si plante) et
    des metriques CPU/memoire echantillonnees via .NET Process. Ce n'est PAS le FPS
    reel du jeu (DolphinJet a son propre compteur FPS a l'ecran, mais rien ne
    l'exporte en dehors du rendu) - c'est un proxy honnete sur "le process est-il
    fige, boucle-t-il a fond sur un coeur, fuit-il de la memoire", pas une mesure
    de fluidite en jeu.

    Preuve de niveau SCRIPTED uniquement (voir la discipline de preuve du projet).

.EXAMPLE
    powershell -File tools/port/dolphinjet_probe.ps1 -Exe build-msvc/RelWithDebInfo/dolphinjet.exe -DurationSeconds 90
#>
param(
    [Parameter(Mandatory = $true)][string]$Exe,
    [int]$DurationSeconds = 90,
    [int]$SampleIntervalSeconds = 3,
    [string]$CsvOut = "dolphinjet_probe.csv",
    [string]$StdoutLog = $null,
    [string]$StderrLog = $null
)

if (-not (Test-Path $Exe)) {
    Write-Error "introuvable : $Exe"
    exit 2
}

if (-not $StdoutLog) { $StdoutLog = Join-Path $env:TEMP "dolphinjet_probe_stdout.log" }
if (-not $StderrLog) { $StderrLog = Join-Path $env:TEMP "dolphinjet_probe_stderr.log" }

$exeDir = Split-Path -Parent (Resolve-Path $Exe)
Push-Location $exeDir
$exeName = Split-Path -Leaf $Exe

Write-Host "[probe] lancement : $exeName"
$proc = Start-Process -FilePath ".\$exeName" -PassThru `
    -RedirectStandardOutput $StdoutLog -RedirectStandardError $StderrLog
Pop-Location

$samples = New-Object System.Collections.Generic.List[object]
$crashed = $false
$exitCode = $null
$start = Get-Date
$deadline = $start.AddSeconds($DurationSeconds)

while ((Get-Date) -lt $deadline) {
    Start-Sleep -Seconds $SampleIntervalSeconds
    $proc.Refresh()
    if ($proc.HasExited) {
        $crashed = $true
        $exitCode = $proc.ExitCode
        break
    }
    $t = [math]::Round(((Get-Date) - $start).TotalSeconds, 1)
    $cpuSeconds = [math]::Round($proc.TotalProcessorTime.TotalSeconds, 2)
    $workingSetMb = [math]::Round($proc.WorkingSet64 / 1MB, 1)
    $threads = $proc.Threads.Count
    $sample = [pscustomobject]@{
        t              = $t
        cpu_seconds    = $cpuSeconds
        working_set_mb = $workingSetMb
        threads        = $threads
        title          = $proc.MainWindowTitle
    }
    $samples.Add($sample) | Out-Null
    Write-Host ("[probe] t={0,6}s cpu_total={1,8}s ram={2,7} Mo threads={3,3}  title={4}" -f `
        $t, $cpuSeconds, $workingSetMb, $threads, $sample.title)
}

if (-not $crashed -and -not $proc.HasExited) {
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
}

if ($samples.Count -gt 0) {
    $samples | Export-Csv -Path $CsvOut -NoTypeInformation
    Write-Host "[probe] $($samples.Count) echantillons ecrits dans $CsvOut"
}

Write-Host ""
Write-Host ("=" * 60)
if ($crashed) {
    Write-Host "CRASH: le process s'est arrete tout seul (code $exitCode, hex 0x$('{0:X}' -f $exitCode))"
    Write-Host "--- fin de stderr ---"
    Get-Content $StderrLog -Tail 30 -ErrorAction SilentlyContinue
    exit 1
} else {
    Write-Host "PAS DE CRASH sur $DurationSeconds s. $($samples.Count) echantillons."
    if ($samples.Count -gt 1) {
        $cpuDelta = $samples[-1].cpu_seconds - $samples[0].cpu_seconds
        $wallDelta = $samples[-1].t - $samples[0].t
        $cpuPct = if ($wallDelta -gt 0) { [math]::Round(100.0 * $cpuDelta / $wallDelta, 1) } else { 0 }
        Write-Host "CPU moyen sur la fenetre mesuree : $cpuPct% d'un coeur"
        Write-Host "RAM : $($samples[0].working_set_mb) Mo -> $($samples[-1].working_set_mb) Mo"
    }
    Write-Host ("=" * 60)
    exit 0
}
