<#
.SYNOPSIS
    Executes benchmark suites for the Windows Performance Investigation Lab.

.DESCRIPTION
    Runs perf_lab.exe with specified parameters and outputs CSV and summary reports.

.PARAMETER Mode
    Workload mode: "baseline", "optimized", or "compare". Default is "compare".

.PARAMETER Threads
    Number of worker threads. Default is 0 (auto-detect hardware concurrency).

.PARAMETER Operations
    Number of operations per worker thread. Default is 1,000,000.

.PARAMETER Runs
    Number of benchmark iterations. Default is 5.

.PARAMETER OutputDir
    Directory path for CSV and summary results. Default is "results".

.PARAMETER Configuration
    Build configuration to locate executable: "Release" or "Debug". Default is "Release".

.EXAMPLE
    .\scripts\run_benchmark.ps1 -Mode compare -Threads 8 -Operations 2000000 -Runs 5
#>

[CmdletBinding()]
param(
    [ValidateSet("baseline", "optimized", "compare")]
    [string]$Mode = "compare",

    [int]$Threads = 0,

    [long]$Operations = 1000000,

    [int]$Runs = 5,

    [string]$OutputDir = "results",

    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

Write-Host "================================================================================" -ForegroundColor Cyan
Write-Host "       Windows Performance Investigation Lab - Benchmark Runner" -ForegroundColor Cyan
Write-Host "================================================================================" -ForegroundColor Cyan

# Locate executable across typical build layouts
$exeCandidates = @(
    "$PSScriptRoot\..\build\$Configuration\perf_lab.exe",
    "$PSScriptRoot\..\build\perf_lab.exe",
    "$PSScriptRoot\..\build\bin\$Configuration\perf_lab.exe",
    "$PSScriptRoot\..\build\bin\perf_lab.exe"
)

$exePath = $null
foreach ($candidate in $exeCandidates) {
    if (Test-Path $candidate) {
        $exePath = (Resolve-Path $candidate).Path
        break
    }
}

if (-not $exePath) {
    Write-Error "perf_lab.exe not found in build directory. Please run .\scripts\build.ps1 first."
    exit 1
}

Write-Host "[INFO] Using executable: $exePath" -ForegroundColor Gray

# Construct argument array
$appArgs = @("--mode", $Mode, "--operations", $Operations, "--runs", $Runs, "--output-dir", $OutputDir)
if ($Threads -gt 0) {
    $appArgs += @("--threads", $Threads)
}

Write-Host "[INFO] Executing: $exePath $($appArgs -join ' ')" -ForegroundColor Green

& $exePath @appArgs
$exitCode = $LASTEXITCODE

if ($exitCode -ne 0) {
    Write-Error "Benchmark execution failed with exit code $exitCode."
    exit $exitCode
}

Write-Host "`n[SUCCESS] Benchmark run finished. Results stored in '$OutputDir'." -ForegroundColor Green
