param(
    [string]$QtRoot = "D:\Qt\6.11.2\mingw_64",
    [string]$MingwRoot = "D:\Qt\Tools\mingw1310_64",
    [string]$CMakeExe = "D:\Qt\Tools\CMake_64\bin\cmake.exe",
    [string]$BuildDir = "",
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string]$Config = "Debug"
)

$ErrorActionPreference = "Stop"

if (-not $BuildDir) {
    $BuildDir = Join-Path (Split-Path $PSScriptRoot -Parent) "build-mingw"
}

$makeExe = Join-Path $MingwRoot "bin\mingw32-make.exe"
$gccExe = Join-Path $MingwRoot "bin\gcc.exe"
$gxxExe = Join-Path $MingwRoot "bin\g++.exe"

foreach ($tool in @($CMakeExe, $makeExe, $gccExe, $gxxExe, (Join-Path $QtRoot "lib\cmake\Qt6\Qt6Config.cmake"))) {
    if (-not (Test-Path $tool)) {
        throw "Missing build dependency: $tool`nUpdate the paths at the top of this script."
    }
}

$env:PATH = "$(Join-Path $MingwRoot 'bin');$(Split-Path $CMakeExe -Parent);$(Join-Path $QtRoot 'bin');$env:PATH"

Write-Host "Configuring MysticTurtle with MinGW Makefiles..." -ForegroundColor Cyan
& $CMakeExe -S "$PSScriptRoot\.." -B $BuildDir `
    -G "MinGW Makefiles" `
    -DCMAKE_BUILD_TYPE=$Config `
    -DCMAKE_PREFIX_PATH=$QtRoot `
    -DCMAKE_C_COMPILER=$gccExe `
    -DCMAKE_CXX_COMPILER=$gxxExe `
    -DCMAKE_MAKE_PROGRAM=$makeExe

if ($LASTEXITCODE -ne 0) {
    throw "CMake configure failed with exit code $LASTEXITCODE."
}

Write-Host "Building MysticTurtle..." -ForegroundColor Cyan
& $CMakeExe --build $BuildDir --parallel

if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE."
}

$exe = Join-Path $BuildDir "Mystic-Turtle-Launcher.exe"
Write-Host ""
Write-Host "Build complete: $exe" -ForegroundColor Green
