param (
    [Parameter(Mandatory=$true, Position=0)]
    [ValidateSet("build", "test", "clean", "help")]
    [string]$Command,

    [switch]$Fast,
    [string]$LLVM_DIR = ""
)

$BuildDir = "build"

function Show-Help {
    Write-Host "Manifast Build Tool" -ForegroundColor Cyan
    Write-Host "Usage: .\manifast.ps1 <command> [options]"
    Write-Host ""
    Write-Host "Commands:"
    Write-Host "  build      Configure and build the project"
    Write-Host "  test       Run the test suite"
    Write-Host "  clean      Remove the build directory"
    Write-Host "  help       Show this help message"
    Write-Host ""
    Write-Host "Options:"
    Write-Host "  --fast     Disable vcpkg LLVM download (uses system LLVM)"
}

if ($Command -eq "help") {
    Show-Help
    exit
}

if ($Command -eq "clean") {
    Write-Host "Cleaning build directory ($BuildDir)..." -ForegroundColor Yellow
    if (Test-Path $BuildDir) {
        Remove-Item $BuildDir -Recurse -Force
    }
    Write-Host "Done." -ForegroundColor Green
    exit
}

if ($Command -eq "build") {
    if (-not (Test-Path $BuildDir)) {
        Write-Host "Configuring Project..." -ForegroundColor Cyan
        
        $Args = @("-S", ".", "-B", $BuildDir, "-G", "Ninja")
        
        # Determine build type (Fast/System LLVM vs Default/Vcpkg)
        if ($Fast) {
             Write-Host "  Mode: FAST (System LLVM)" -ForegroundColor Magenta
             $Args += "-DVCPKG_MANIFEST_FEATURES="
             
             # Fallback to known system paths if needed, but CMake usually finds it
             if ($LLVM_DIR) {
                 $Args += "-DLLVM_DIR=$LLVM_DIR"
             }
        } else {
             Write-Host "  Mode: DEFAULT (Vcpkg Bundle)" -ForegroundColor Blue
             # Default features are used
        }

        # Toolchain defaults (optional overrides)
        # $Args += "-DCMAKE_CXX_COMPILER=D:/Program/msys64/ucrt64/bin/g++.exe"
        
        cmake @Args
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
    
    Write-Host "Building Project..." -ForegroundColor Cyan
    cmake --build $BuildDir
    exit $LASTEXITCODE
}

if ($Command -eq "test") {
    $TestBin = "$BuildDir\bin\manifast.exe"
    if (-not (Test-Path $TestBin)) {
        Write-Host "Error: Binary not found. Run 'build' first." -ForegroundColor Red
        exit 1
    }
    
    Write-Host "Running Tests..." -ForegroundColor Cyan
    & $TestBin tests/Manifast/test.mnf --test tests
    exit $LASTEXITCODE
}
