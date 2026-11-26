# Builds stopwatch.exe and a self-contained folder at dist\stopwatch that runs
# on any 64-bit Windows 10 or 11 without installing Qt.
#
# Run it from a "x64 Native Tools Command Prompt for VS 2022" PowerShell
# (so nmake and the compiler are on PATH) with Qt 6's msvc2022_64 bin folder
# on PATH, e.g. $env:PATH = "C:\Qt\6.8.3\msvc2022_64\bin;$env:PATH".
#
#   powershell -ExecutionPolicy Bypass -File bin\build-windows.ps1 [-Test]

param([switch]$Test)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root "build-windows"
$dist = Join-Path $root "dist\stopwatch"

function Run($exe, [string[]]$arguments) {
    & $exe @arguments
    if ($LASTEXITCODE -ne 0) { throw "$exe failed with exit code $LASTEXITCODE" }
}

# One release build straight into the build folder; qmake's default on
# Windows builds debug and release into subfolders.
New-Item -ItemType Directory -Force $build | Out-Null
Push-Location $build
try {
    Run qmake @((Join-Path $root "stopwatch.pro"), "CONFIG-=debug_and_release", "CONFIG+=release")
    Run nmake @("/nologo")
} finally { Pop-Location }

if ($Test) {
    $tests = Join-Path $build "tests"
    New-Item -ItemType Directory -Force $tests | Out-Null
    Push-Location $tests
    try {
        Run qmake @((Join-Path $root "tests\stopwatch_tests.pro"), "CONFIG-=debug_and_release", "CONFIG+=release")
        Run nmake @("/nologo")
        # The results go to a file and are printed from there, so a run that
        # prints nothing can't pass unnoticed.
        $env:QT_QPA_PLATFORM = "offscreen"
        $results = Join-Path $tests "results.txt"
        if (Test-Path $results) { Remove-Item $results }
        & (Join-Path $tests "stopwatch_tests.exe") -o "$results,txt"
        $code = $LASTEXITCODE
        Remove-Item Env:\QT_QPA_PLATFORM
        if (-not (Test-Path $results)) { throw "The tests didn't run (exit code $code)" }
        Get-Content $results | Write-Host
        if ($code -ne 0 -or -not (Select-String -Path $results -Pattern "Totals: \d+ passed, 0 failed" -Quiet)) {
            throw "Tests failed (exit code $code)"
        }
    } finally { Pop-Location }
}

# Copy in only what the app loads. The shader compilers and the software
# OpenGL fallback are tens of megabytes the app never uses: Qt Quick's
# shaders are precompiled and it draws through Direct3D 11.
if (Test-Path $dist) { Remove-Item -Recurse -Force $dist }
New-Item -ItemType Directory -Force $dist | Out-Null
Copy-Item (Join-Path $build "stopwatch.exe") $dist
Run windeployqt @(
    "--release", "--qmldir", (Join-Path $root "src"),
    "--no-translations", "--no-system-d3d-compiler", "--no-system-dxc-compiler", "--no-opengl-sw",
    "--no-compiler-runtime", "--skip-plugin-types", "generic,networkinformation,tls,iconengines",
    (Join-Path $dist "stopwatch.exe"))

# The Visual C++ runtime goes beside the exe, so the folder runs on a machine
# that never installed the redistributable.
if ($env:VCToolsRedistDir) {
    Get-ChildItem -Path (Join-Path $env:VCToolsRedistDir "x64") -Directory -Filter "Microsoft.VC*.CRT" |
        ForEach-Object { Copy-Item (Join-Path $_.FullName "*.dll") $dist }
}

Copy-Item (Join-Path $root "LICENSE") $dist
$size = (Get-ChildItem -Recurse $dist | Measure-Object -Property Length -Sum).Sum / 1MB
Write-Host ("Built {0} ({1:N1} MB)" -f $dist, $size)
