<#
.SYNOPSIS
    Captures an ETW performance trace of perf_lab.exe using Windows Performance Recorder (WPR).

.DESCRIPTION
    Automates tracing of CPU scheduling, context switches, and application TraceLogging
    events using config\PerformanceLab.wprp. Ensures safe start, run, stop, and cleanup
    handling so that recording sessions are never left active.

.PARAMETER Mode
    Workload mode to trace: "baseline" or "optimized". Default is "baseline".

.PARAMETER Threads
    Number of worker threads. Default is 0 (auto-detect hardware concurrency).

.PARAMETER Operations
    Number of operations per worker thread. Default is 1,000,000.

.PARAMETER OutputFile
    Path for the output ETL trace file. Default is "results\trace_<mode>.etl".

.PARAMETER Configuration
    Build configuration to locate executable: "Release" or "Debug". Default is "Release".

.EXAMPLE
    .\scripts\capture_trace.ps1 -Mode baseline -Threads 8 -Operations 1000000
#>

[CmdletBinding()]
param(
    [ValidateSet("baseline", "optimized")]
    [string]$Mode = "baseline",

    [int]$Threads = 0,

    [long]$Operations = 1000000,

    [string]$OutputFile = "",

    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

Write-Host "================================================================================" -ForegroundColor Cyan
Write-Host "    Windows Performance Investigation Lab - WPR Trace Capture" -ForegroundColor Cyan
Write-Host "================================================================================" -ForegroundColor Cyan

# 1. Locate wpr.exe
$wprExe = $null
if (Get-Command "wpr.exe" -ErrorAction SilentlyContinue) {
    $wprExe = "wpr.exe"
} else {
    $wprSearchPaths = @(
        "$env:SystemRoot\System32\wpr.exe",
        "C:\Program Files (x86)\Windows Kits\10\bin\*\x64\wpr.exe",
        "C:\Program Files (x86)\Windows Kits\11\bin\*\x64\wpr.exe",
        "C:\Program Files\Windows Kits\10\bin\*\x64\wpr.exe"
    )
    foreach ($pattern in $wprSearchPaths) {
        $found = Get-Item $pattern -ErrorAction SilentlyContinue | Select-Object -Last 1
        if ($found) {
            $wprExe = $found.FullName
            break
        }
    }
}

if (-not $wprExe) {
    Write-Error @"
Windows Performance Recorder (wpr.exe) was not found on this machine.
WPR is included with Windows 10/11 or as part of the Windows Performance Toolkit (WPT)
in the Windows Assessment and Deployment Kit (ADK).
Please verify that WPR is installed and accessible in PATH.
"@
    exit 1
}

Write-Host "[INFO] WPR Utility: $wprExe" -ForegroundColor Gray

# 2. Verify WPR Profile exists
$repoRoot = (Resolve-Path "$PSScriptRoot\..").Path
$profilePath = Join-Path $repoRoot "config\PerformanceLab.wprp"
if (-not (Test-Path $profilePath)) {
    Write-Error "WPR recording profile not found at '$profilePath'."
    exit 1
}

Write-Host "[INFO] WPR Profile: $profilePath" -ForegroundColor Gray

# 3. Resolve output file path
$outputDir = Join-Path $repoRoot "results"
if (-not (Test-Path $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
}

if (-not $OutputFile) {
    $OutputFile = Join-Path $outputDir "trace_${Mode}.etl"
} else {
    $OutputFile = [System.IO.Path]::GetFullPath($OutputFile)
}

# 4. Locate perf_lab.exe
$exeCandidates = @(
    Join-Path $repoRoot "build\$Configuration\perf_lab.exe",
    Join-Path $repoRoot "build\perf_lab.exe",
    Join-Path $repoRoot "build\bin\$Configuration\perf_lab.exe",
    Join-Path $repoRoot "build\bin\perf_lab.exe"
)

$exePath = $null
foreach ($candidate in $exeCandidates) {
    if (Test-Path $candidate) {
        $exePath = (Resolve-Path $candidate).Path
        break
    }
}

if (-not $exePath) {
    Write-Error "perf_lab.exe not found. Please run .\scripts\build.ps1 first."
    exit 1
}

# 5. Check if an existing WPR recording is active
Write-Host "[INFO] Checking existing WPR recording sessions..." -ForegroundColor Yellow
$statusOutput = & $wprExe -status 2>&1 | Out-String
if ($statusOutput -match "WPR is recording") {
    Write-Host "[WARN] An active WPR recording session was detected. Canceling previous session..." -ForegroundColor Yellow
    & $wprExe -cancel | Out-Null
}

# 6. Start WPR recording with custom profile
Write-Host "[INFO] Starting WPR trace capture (Profile: PerformanceLabProfile!Verbose!File)..." -ForegroundColor Green
$startArgs = @("-start", "$profilePath!PerformanceLabProfile", "-filemode")

$recordingActive = $false
try {
    & $wprExe @startArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to start WPR recording. Ensure the script is running with necessary privileges."
    }
    $recordingActive = $true

    # 7. Execute workload under trace
    Write-Host "[INFO] Executing workload '$Mode' under active trace..." -ForegroundColor Green
    $runArgs = @("--mode", $Mode, "--operations", $Operations, "--runs", 1, "--output-dir", $outputDir)
    if ($Threads -gt 0) {
        $runArgs += @("--threads", $Threads)
    }

    & $exePath @runArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Workload execution failed during trace capture with code $LASTEXITCODE."
    }

    # 8. Stop WPR recording and write ETL file
    Write-Host "[INFO] Stopping WPR trace and saving to '$OutputFile'..." -ForegroundColor Green
    if (Test-Path $OutputFile) {
        Remove-Item -Force $OutputFile
    }

    $stopArgs = @("-stop", $OutputFile, "PerformanceLab $Mode Trace")
    & $wprExe @stopArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to stop WPR recording (Exit code: $LASTEXITCODE)."
    }
    $recordingActive = $false

    Write-Host "`n[SUCCESS] Trace captured successfully!" -ForegroundColor Green
    Write-Host "Trace File: $OutputFile" -ForegroundColor Cyan
    Write-Host @"

NEXT STEPS FOR ANALYSIS:
1. Open Windows Performance Analyzer:
   wpa.exe "$OutputFile"

2. Verify Application TraceLogging Events:
   In WPA, open 'System Activity' -> 'Generic Events'
   Look for Provider: Shivam.PerformanceInvestigationLab
   Verify WorkloadStarted, WorkerStarted, WorkerCompleted, WorkloadCompleted events.

3. Inspect Synchronization Contention:
   In WPA, open 'Computation' -> 'CPU Usage (Precise)'
   Group by Process -> Thread -> Wait Function
   Inspect context switch count and time spent in NtWaitForAlertByThreadId / WaitOnAddress.
"@ -ForegroundColor Yellow

} catch {
    Write-Error "[ERROR] Trace capture failed: $_"
    if ($recordingActive) {
        Write-Host "[INFO] Safely canceling active WPR session..." -ForegroundColor Yellow
        & $wprExe -cancel | Out-Null
    }
    exit 1
}
