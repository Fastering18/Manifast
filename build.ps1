param (
    [switch]$Clean,
    [switch]$Run
)

if ($Clean) {
    Write-Host "Cleaning build directory..."
    if (Test-Path "build") {
        Remove-Item "build" -Recurse -Force
    }
}

if (-not (Test-Path "build")) {
    Write-Host "Configuring..."
    cmake -S . -B build -G "Ninja" -DCMAKE_CXX_COMPILER="D:/Program/msys64/ucrt64/bin/g++.exe" -DCMAKE_MAKE_PROGRAM="C:/Strawberry/c/bin/ninja.exe"
}

Write-Host "Building..."
cmake --build build

if ($LASTEXITCODE -eq 0 -and $Run) {
    Write-Host "Running Test..."
    .\build\bin\manifast.exe tests/Manifast/test.mnf
}
