$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

cmake -S . -B build -A x64 -DBUILD_TESTING=ON
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }

cmake --build build --config Release --target AudioViz_PC pc_processor_test
if ($LASTEXITCODE -ne 0) { throw "Windows build failed" }

ctest --test-dir build -C Release --output-on-failure -R pc_processor_test
if ($LASTEXITCODE -ne 0) { throw "Windows test failed" }

$app = Get-ChildItem -Path build -Recurse -Filter AudioViz_PC.exe |
    Where-Object { $_.FullName -match "Release" } |
    Select-Object -First 1
if (-not $app) { throw "AudioViz_PC.exe not found under build" }
Write-Host "Ready: $($app.FullName)"
