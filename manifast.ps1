param (
    [Parameter(Mandatory=$true, Position=0)]
    [ValidateSet("build", "test", "clean", "help")]
    [string]$Command,

    [switch]$Fast,
    [string]$LLVM_DIR = "",

    [Parameter(ValueFromRemainingArguments=$true)]
    $RemainingArgs
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

# Check for --fast or -Fast in $RemainingArgs manually to support both styles
foreach ($arg in $RemainingArgs) {
    if ($arg -match "^--?fast$") {
        $Fast = $true
    }
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
    # If build directory exists but is broken (no ninja file), or if we want to be safe,
    # we should allow re-configuration.
    $NeedsConfig = $true
    $ModeFile = "$BuildDir\build_mode.txt"
    $CurrentMode = if ($Fast) { "FAST" } else { "DEFAULT" }

    if (Test-Path "$BuildDir/build.ninja") {
        # Check if mode changed
        if (Test-Path $ModeFile) {
            $LastMode = Get-Content $ModeFile
            if ($LastMode -eq $CurrentMode) {
                $NeedsConfig = $false
            }
        }
    }

    if ($NeedsConfig) {
        if (-not (Test-Path $BuildDir)) { New-Item -ItemType Directory -Path $BuildDir }
        $CurrentMode | Out-File $ModeFile
        
        Write-Host "Configuring Project..." -ForegroundColor Cyan
        
        $Args = @("-S", ".", "-B", $BuildDir, "-G", "Ninja")
        
        # Toolchain detection (vcpkg)
        $VcpkgToolchain = "$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
        if (-not (Test-Path $VcpkgToolchain)) {
            $VcpkgToolchain = "C:\vcpkg\scripts\buildsystems\vcpkg.cmake"
        }
        
        # Determine build type (Fast/System LLVM vs Default/Vcpkg)
        if ($Fast) {
             # --- FAST MODE: Use System Libraries (MSYS2/etc) ---
             Write-Host "  Mode: FAST (System LLVM & Libs)" -ForegroundColor Magenta
             $Args += "-DVCPKG_MANIFEST_FEATURES="
             
             # Do NOT use vcpkg toolchain in Fast mode as it hides system libs
             # and leads to ABI mismatch (MSVC vs MinGW)
             
             if ($LLVM_DIR) {
                 $Args += "-DLLVM_DIR=$LLVM_DIR"
             } elseif (Test-Path "D:\Program\msys64\ucrt64\lib\cmake\llvm") {
                 $Args += "-DLLVM_DIR=D:\Program\msys64\ucrt64\lib\cmake\llvm"
             } elseif (Test-Path "D:\Program\LLVM") {
                 $Args += "-DLLVM_DIR=D:\Program\LLVM\lib\cmake\llvm"
             }
        } else {
             # --- DEFAULT MODE: Use Vcpkg ---
             Write-Host "  Mode: DEFAULT (Vcpkg Bundle)" -ForegroundColor Blue
             if (Test-Path $VcpkgToolchain) {
                 Write-Host "  Using Toolchain: $VcpkgToolchain" -ForegroundColor Gray
                 $Args += "-DCMAKE_TOOLCHAIN_FILE=$VcpkgToolchain"
             }
        }

        if (Test-Path $BuildDir) {
            Write-Host "  Cleaning existing cache..." -ForegroundColor Gray
            Remove-Item "$BuildDir\CMakeCache.txt" -ErrorAction SilentlyContinue
        }
        
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
