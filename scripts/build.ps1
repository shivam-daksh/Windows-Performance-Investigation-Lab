<#
.SYNOPSIS
    Builds the Windows Performance Investigation Lab using CMake and MSVC.

.DESCRIPTION
    Configures CMake, compiles the project in Release or Debug mode, and optionally
    executes CTest test suites.

.PARAMETER Configuration
    Build configuration: "Release", "Debug", or "RelWithDebInfo". Default is "Release".

.PARAMETER Clean
    If specified, deletes the existing build directory before configuring.

.PARAMETER BuildDir
    Path to the build output directory. Default is "build".

.PARAMETER RunTests
    If specified, runs CTest test suites upon successful compilation.

.EXAMPLE
    .\scripts\build.ps1 -Configuration Release -RunTests
#>

[CmdletBinding()]
param(
    [ValidateSet("Release", "Debug", "RelWithDebInfo")]
    [string]$Configuration = "Release",

    [switch]$Clean,

    [string]$BuildDir = "build",

    [switch]$RunTests
)

$ErrorActionPreference = "Stop"

Write-Host "================================================================================" -ForegroundColor Cyan
Write-Host "         Windows Performance Investigation Lab - Build System" -ForegroundColor Cyan
Write-Host "================================================================================" -ForegroundColor Cyan

# 1. Verify CMake is available
if (-not (Get-Command "cmake" -ErrorAction SilentlyContinue)) {
    Write-Error "CMake is not found in PATH. Please install CMake 3.20+ or run from a Visual Studio Developer Command Prompt."
    exit 1
}

# 2. Clean build directory if requested
if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "[INFO] Cleaning build directory: $BuildDir" -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir
}

# 3. Configure CMake
Write-Host "[INFO] Configuring CMake ($Configuration)..." -ForegroundColor Green
$configureArgs = @("-B", $BuildDir, "-S", ".", "-DCMAKE_BUILD_TYPE=$Configuration")
& cmake @configureArgs
if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake configuration failed with exit code $LASTEXITCODE."
    exit $LASTEXITCODE
}

# 4. Build solution
Write-Host "[INFO] Compiling project..." -ForegroundColor Green
$buildArgs = @("--build", $BuildDir, "--config", $Configuration)
& cmake @buildArgs
if ($LASTEXITCODE -ne 0) {
    Write-Error "Build compilation failed with exit code $LASTEXITCODE."
    exit $LASTEXITCODE
}

Write-Host "[SUCCESS] Build completed successfully." -ForegroundColor Green

# 5. Execute CTest if requested
if ($RunTests) {
    Write-Host "`n[INFO] Executing CTest unit test suite..." -ForegroundColor Cyan
    $testArgs = @("--test-dir", $BuildDir, "-C", $Configuration, "--output-on-failure")
    & ctest @testArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Unit tests failed with exit code $LASTEXITCODE."
        exit $LASTEXITCODE
    }
    Write-Host "[SUCCESS] All unit tests passed." -ForegroundColor Green
}
