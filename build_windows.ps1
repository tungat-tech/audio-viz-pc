$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }

cmake --build build --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw "Windows build failed" }

ctest --test-dir build -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "Windows test failed" }

$app = Join-Path $PSScriptRoot "build/Release/AudioViz_PC.exe"
if (-not (Test-Path $app)) { throw "AudioViz_PC.exe not found under build/Release" }
Write-Host "Ready: $app"
