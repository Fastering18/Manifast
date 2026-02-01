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
        if ($Fast) {
            # If we want FAST (MinGW) but find Ninja, we MUST re-config
            $NeedsConfig = $true
        } else {
            # Check if mode changed
            if (Test-Path $ModeFile) {
                $LastMode = Get-Content $ModeFile
                if ($LastMode -eq $CurrentMode) {
                    $NeedsConfig = $false
                }
            }
        }
    }

    if ($NeedsConfig) {
        Write-Host "  Mode change or missing config detected. Forcing clean state..." -ForegroundColor Gray
        
        # Kill any hung processes that might be locking the build dir
        taskkill /F /IM ninja.exe /T 2>$null
        taskkill /F /IM mingw32-make.exe /T 2>$null
        taskkill /F /IM manifast.exe /T 2>$null
        taskkill /F /IM manifast_tests.exe /T 2>$null
        
        if (Test-Path $BuildDir) { 
            Write-Host "  Wiping build directory..." -ForegroundColor Gray
            Remove-Item $BuildDir -Recurse -Force -ErrorAction SilentlyContinue
            if (Test-Path $BuildDir) {
                Write-Host "  Warning: Build directory still exists! Manual deletion might be required." -ForegroundColor Yellow
            }
        }
        if (-not (Test-Path $BuildDir)) {
            New-Item -ItemType Directory -Path $BuildDir | Out-Null
        }
        $CurrentMode | Out-File $ModeFile
        
        Write-Host "Configuring Project..." -ForegroundColor Cyan
        
        $CMakeArgs = @("-S", ".", "-B", $BuildDir, "-G", "Ninja")
        
        # Toolchain detection (vcpkg)
        $VcpkgToolchain = "$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
        if (-not (Test-Path $VcpkgToolchain)) {
            $VcpkgToolchain = "C:\vcpkg\scripts\buildsystems\vcpkg.cmake"
        }
        
        # Determine build type (Fast/System LLVM vs Default/Vcpkg)
        if ($Fast) {
             # --- FAST MODE: Use System Libraries (MSYS2/etc) ---
             Write-Host "  Mode: FAST (System LLVM & Libs)" -ForegroundColor Magenta
             
             # Force MSYS2 UCRT64 paths to avoid picking up Strawberry/Anaconda
             $MsysPath = "D:\Program\msys64\ucrt64"
             if (Test-Path $MsysPath) {
                 Write-Host "  Forcing MSYS2 UCRT64 Toolchain..." -ForegroundColor Gray
                 
                 # Prepend MSYS2 bin to PATH for this process
                 $env:PATH = "$MsysPath\bin;" + $env:PATH
                 $env:CC = "$MsysPath\bin\gcc.exe"
                 $env:CXX = "$MsysPath\bin\g++.exe"
                 
                 Write-Host "  Running CMake configuration (MinGW Makefiles)..." -ForegroundColor Gray
                 
                 # Using a flat string to avoid backtick issues
                 $cmake_cmd = "cmake -S . -B $BuildDir -G `"MinGW Makefiles`" -DVCPKG_MANIFEST_FEATURES=`"`" -DCMAKE_C_COMPILER=`"$MsysPath\bin\gcc.exe`" -DCMAKE_CXX_COMPILER=`"$MsysPath\bin\g++.exe`" -DCMAKE_LINKER=`"$MsysPath\bin\ld.exe`" -DCMAKE_AR=`"$MsysPath\bin\ar.exe`" -DCMAKE_RANLIB=`"$MsysPath\bin\ranlib.exe`" -DCMAKE_MAKE_PROGRAM=`"$MsysPath\bin\mingw32-make.exe`" -DCMAKE_PREFIX_PATH=`"$MsysPath`" -DCMAKE_FIND_ROOT_PATH=`"$MsysPath`" -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY -DCMAKE_IGNORE_PATH=`"D:/Program/Anaconda`" -DLLVM_DIR=`"$MsysPath/lib/cmake/llvm`" -DGTest_DIR=`"$MsysPath/lib/cmake/GTest`" -Dfmt_DIR=`"$MsysPath/lib/cmake/fmt`""
                 
                 Invoke-Expression $cmake_cmd
             } elseif ($LLVM_DIR) {
                 cmake -S . -B $BuildDir -G Ninja -DLLVM_DIR=$LLVM_DIR
             } else {
                 cmake -S . -B $BuildDir -G Ninja
             }
        } else {
             # --- DEFAULT MODE: Use Vcpkg ---
             Write-Host "  Mode: DEFAULT (Vcpkg Bundle)" -ForegroundColor Blue
             if (Test-Path $VcpkgToolchain) {
                 Write-Host "  Using Toolchain: $VcpkgToolchain" -ForegroundColor Gray
                 cmake -S . -B $BuildDir -G Ninja -DCMAKE_TOOLCHAIN_FILE=$VcpkgToolchain
             } else {
                 cmake -S . -B $BuildDir -G Ninja
             }
        }

        if ($LASTEXITCODE -ne 0) { 
            Write-Host "Error: CMake configuration failed with exit code $LASTEXITCODE" -ForegroundColor Red
            exit $LASTEXITCODE 
        }
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
