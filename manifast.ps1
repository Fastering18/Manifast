param (
    [Parameter(Mandatory=$true, Position=0)]
    [ValidateSet("build", "run", "run-vm", "test", "clean", "help", "build-wasm", "install", "uninstall", "bootstrap", "check", "install-hooks")]
    [string]$Command,

    [switch]$Fast,
    [string]$LLVM_DIR = "",
    [string]$Preset = "",

    [Parameter(ValueFromRemainingArguments=$true)]
    $RemainingArgs
)

$ErrorActionPreference = "Continue"
$BuildDir = "build"
$ScriptRoot = $PSScriptRoot
if (-not $ScriptRoot) { $ScriptRoot = Get-Location }

function Show-Help {
    Write-Host "Manifast Build Tool (Windows)" -ForegroundColor Cyan
    Write-Host "Usage: .\manifast.ps1 <command> [options]"
    Write-Host ""
    Write-Host "Commands:"
    Write-Host "  bootstrap      Install toolchain via winget/MSYS2 (non-GUI)"
    Write-Host "  build          Configure and build the project"
    Write-Host "  run            Run manifast file in jit tier"
    Write-Host "  run-vm         Run manifast file in vm tier"
    Write-Host "  test           Run the test suite"
    Write-Host "  check          Pre-push gate: tests + rebuild WASM into docs/"
    Write-Host "  install-hooks  Enable versioned git pre-push hook"
    Write-Host "  install        Install binaries to user profile and add to PATH"
    Write-Host "  uninstall      Remove binaries from system and clear from PATH"
    Write-Host "  clean          Remove the build directory"
    Write-Host "  build-wasm     Build for WebAssembly (requires Emscripten)"
    Write-Host "  help           Show this help message"
    Write-Host ""
    Write-Host "Options:"
    Write-Host "  --fast              Use system/MSYS2 LLVM (skip vcpkg LLVM)"
    Write-Host "  -Preset <name>      CMake preset (windows-msys2, no-llvm, vcpkg, ...)"
    Write-Host "  -LLVM_DIR <path>    Explicit LLVMConfig.cmake directory"
}

function Find-Msys2Ucrt {
    if ($env:MSYSTEM_PREFIX -and (Test-Path "$env:MSYSTEM_PREFIX\bin\g++.exe")) {
        return $env:MSYSTEM_PREFIX
    }
    $roots = @()
    if ($env:MSYS2_ROOT) { $roots += $env:MSYS2_ROOT }
    $roots += @("C:\msys64", "D:\msys64", "C:\tools\msys64")
    foreach ($root in $roots) {
        foreach ($sub in @("ucrt64", "mingw64")) {
            $p = Join-Path $root $sub
            if (Test-Path (Join-Path $p "bin\g++.exe")) { return $p }
        }
    }
    return $null
}

function Find-VcpkgToolchain {
    if ($env:VCPKG_ROOT) {
        $t = Join-Path $env:VCPKG_ROOT "scripts\buildsystems\vcpkg.cmake"
        if (Test-Path $t) { return $t }
    }
    $vcpkg = Get-Command vcpkg -ErrorAction SilentlyContinue
    if ($vcpkg) {
        $root = Split-Path (Split-Path $vcpkg.Source)
        $t = Join-Path $root "scripts\buildsystems\vcpkg.cmake"
        if (Test-Path $t) { return $t }
    }
    foreach ($c in @("C:\vcpkg\scripts\buildsystems\vcpkg.cmake", "$env:USERPROFILE\vcpkg\scripts\buildsystems\vcpkg.cmake")) {
        if (Test-Path $c) { return $c }
    }
    return $null
}

function Find-EmsdkEnv {
    if ($env:EMSDK) {
        $bat = Join-Path $env:EMSDK "emsdk_env.bat"
        if (Test-Path $bat) { return $bat }
    }
    $emcmake = Get-Command emcmake -ErrorAction SilentlyContinue
    if ($emcmake) { return $null } # already on PATH
    foreach ($c in @(
        "C:\emsdk\emsdk_env.bat",
        "D:\emsdk\emsdk_env.bat",
        "$env:USERPROFILE\emsdk\emsdk_env.bat"
    )) {
        if (Test-Path $c) { return $c }
    }
    return $null
}

function Invoke-WithEmsdk([scriptblock]$Action) {
    $emsdkEnv = Find-EmsdkEnv
    if (Get-Command emcmake -ErrorAction SilentlyContinue) {
        & $Action
        return $LASTEXITCODE
    }
    if ($emsdkEnv) {
        $env:EMSDK = Split-Path $emsdkEnv
        # Activate EMSDK in cmd then re-enter PowerShell for the build steps is awkward;
        # callers that need activation should use cmd wrappers. Here we prepend typical paths.
        $upstream = Join-Path (Split-Path $emsdkEnv) "upstream\emscripten"
        $nodeDirs = Get-ChildItem (Join-Path (Split-Path $emsdkEnv) "node") -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending
        if (Test-Path $upstream) { $env:PATH = "$upstream;" + $env:PATH }
        if ($nodeDirs) { $env:PATH = "$($nodeDirs[0].FullName);" + $env:PATH }
        $env:PATH = "$(Split-Path $emsdkEnv);" + $env:PATH
        & $Action
        return $LASTEXITCODE
    }
    Write-Host "Error: Emscripten not found. Set EMSDK or install to C:\emsdk." -ForegroundColor Red
    return 1
}

function Ensure-Cmake {
    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
        Write-Host "Error: cmake not found. Run: .\manifast.ps1 bootstrap" -ForegroundColor Red
        exit 1
    }
}

foreach ($arg in $RemainingArgs) {
    if ($arg -match "^--?fast$") { $Fast = $true }
}

if ($Command -eq "help") {
    Show-Help
    exit 0
}

if ($Command -eq "bootstrap") {
    & "$ScriptRoot\scripts\bootstrap-windows.ps1" @RemainingArgs
    exit $LASTEXITCODE
}

if ($Command -eq "install-hooks") {
    & "$ScriptRoot\scripts\install-hooks.ps1"
    exit $LASTEXITCODE
}

if ($Command -eq "check") {
    & "$ScriptRoot\scripts\check-before-push.ps1" @RemainingArgs
    exit $LASTEXITCODE
}

if ($Command -eq "run") {
    $TestBin = "$BuildDir\bin\mifast.exe"
    if (-not (Test-Path $TestBin)) {
        Write-Host "Error: Binary not found. Run 'build' first." -ForegroundColor Red
        exit 1
    }
    & $TestBin run @RemainingArgs
    exit $LASTEXITCODE
}

if ($Command -eq "run-vm") {
    $TestBin = "$BuildDir\bin\mifast.exe"
    if (-not (Test-Path $TestBin)) {
        Write-Host "Error: Binary not found. Run 'build' first." -ForegroundColor Red
        exit 1
    }
    & $TestBin run @RemainingArgs --vm
    exit $LASTEXITCODE
}

if ($Command -eq "clean") {
    Write-Host "Cleaning build directory ($BuildDir)..." -ForegroundColor Yellow
    if (Test-Path $BuildDir) {
        Remove-Item $BuildDir -Recurse -Force
    }
    Write-Host "Done." -ForegroundColor Green
    exit 0
}

if ($Command -eq "build") {
    Ensure-Cmake

    $NeedsConfig = $true
    $ModeFile = "$BuildDir\build_mode.txt"
    $CurrentMode = if ($Preset) { "PRESET:$Preset" } elseif ($Fast) { "FAST" } else { "DEFAULT" }

    if ((Test-Path "$BuildDir\build.ninja") -or (Test-Path "$BuildDir\CMakeCache.txt")) {
        if (Test-Path $ModeFile) {
            $LastMode = (Get-Content $ModeFile -Raw).Trim()
            if ($LastMode -eq $CurrentMode) { $NeedsConfig = $false }
        }
    }

    if ($NeedsConfig) {
        Write-Host "Configuring Project..." -ForegroundColor Cyan
        if (-not (Test-Path $BuildDir)) {
            New-Item -ItemType Directory -Path $BuildDir | Out-Null
        }
        $CurrentMode | Out-File $ModeFile -Encoding utf8 -NoNewline

        if ($Preset) {
            Write-Host "  Mode: preset $Preset" -ForegroundColor Magenta
            cmake --preset $Preset
            if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        } elseif ($Fast) {
            Write-Host "  Mode: FAST (system / MSYS2 LLVM)" -ForegroundColor Magenta
            $MsysPrefix = Find-Msys2Ucrt
            $cmakeArgs = @("-S", ".", "-B", $BuildDir, "-G", "Ninja", "-DVCPKG_MANIFEST_FEATURES=")

            if ($MsysPrefix) {
                Write-Host "  Using MSYS2 prefix: $MsysPrefix" -ForegroundColor Gray
                $env:PATH = "$MsysPrefix\bin;" + $env:PATH
                $env:CC = "$MsysPrefix\bin\gcc.exe"
                $env:CXX = "$MsysPrefix\bin\g++.exe"
                $cmakeArgs += @(
                    "-DCMAKE_C_COMPILER=$MsysPrefix\bin\gcc.exe",
                    "-DCMAKE_CXX_COMPILER=$MsysPrefix\bin\g++.exe",
                    "-DCMAKE_PREFIX_PATH=$MsysPrefix",
                    "-DLLVM_DIR=$MsysPrefix\lib\cmake\llvm"
                )
                if (Test-Path "$MsysPrefix\lib\cmake\GTest") {
                    $cmakeArgs += "-DGTest_DIR=$MsysPrefix\lib\cmake\GTest"
                }
                if (Test-Path "$MsysPrefix\lib\cmake\fmt") {
                    $cmakeArgs += "-Dfmt_DIR=$MsysPrefix\lib\cmake\fmt"
                }
            } elseif ($LLVM_DIR) {
                $cmakeArgs += "-DLLVM_DIR=$LLVM_DIR"
            } else {
                Write-Host "  No MSYS2 found; relying on PATH / -DLLVM_DIR" -ForegroundColor Yellow
                Write-Host "  Tip: .\manifast.ps1 bootstrap" -ForegroundColor Yellow
            }
            cmake @cmakeArgs
            if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        } else {
            Write-Host "  Mode: DEFAULT (vcpkg if available)" -ForegroundColor Blue
            $toolchain = Find-VcpkgToolchain
            $cmakeArgs = @("-S", ".", "-B", $BuildDir, "-G", "Ninja")
            if ($toolchain) {
                Write-Host "  Using vcpkg: $toolchain" -ForegroundColor Gray
                $cmakeArgs += @(
                    "-DCMAKE_TOOLCHAIN_FILE=$toolchain",
                    "-DVCPKG_TARGET_TRIPLET=x64-windows"
                )
            } else {
                Write-Host "  vcpkg not found; configuring with system packages" -ForegroundColor Yellow
                Write-Host "  Tip: .\manifast.ps1 bootstrap   or   build --fast" -ForegroundColor Yellow
            }
            if ($LLVM_DIR) { $cmakeArgs += "-DLLVM_DIR=$LLVM_DIR" }
            cmake @cmakeArgs
            if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        }
    }

    Write-Host "Building Project..." -ForegroundColor Cyan
    $jobs = [Math]::Max(1, [int]([Environment]::ProcessorCount * 0.75))
    cmake --build $BuildDir --parallel $jobs
    exit $LASTEXITCODE
}

if ($Command -eq "test") {
    $TestBin = "$BuildDir\bin\mifast.exe"
    if (-not (Test-Path $TestBin)) {
        Write-Host "Error: Binary not found. Run 'build' first." -ForegroundColor Red
        exit 1
    }
    Write-Host "Running Modern Test Suite..." -ForegroundColor Cyan
    & $TestBin test @RemainingArgs
    $scriptExit = $LASTEXITCODE
    if (Get-Command ctest -ErrorAction SilentlyContinue) {
        Write-Host "Running CTest..." -ForegroundColor Cyan
        ctest --test-dir $BuildDir --output-on-failure
        if ($LASTEXITCODE -ne 0) { $scriptExit = $LASTEXITCODE }
    }
    exit $scriptExit
}

if ($Command -eq "install") {
    $BinDir = "$BuildDir\bin"
    $LibDir = "$BuildDir\lib"

    if (-not (Test-Path "$BinDir\mifast.exe")) {
        Write-Host "Error: Binary not found. Run 'build' first." -ForegroundColor Red
        exit 1
    }

    $InstallDir = "$env:LOCALAPPDATA\Manifast"
    $InstallBin = "$InstallDir\bin"
    $InstallLib = "$InstallDir\lib"

    Write-Host "Installing Manifast to $InstallDir..." -ForegroundColor Cyan
    New-Item -ItemType Directory -Path $InstallBin -Force | Out-Null
    New-Item -ItemType Directory -Path $InstallLib -Force | Out-Null

    Copy-Item "$BinDir\mifast.exe" "$InstallBin\" -Force
    if (Test-Path "$BinDir\mifastc.exe") {
        Copy-Item "$BinDir\mifastc.exe" "$InstallBin\" -Force
    }

    Get-ChildItem "$LibDir\*" -Include "*.a","*.lib" -ErrorAction SilentlyContinue | ForEach-Object {
        Copy-Item $_.FullName "$InstallLib\" -Force
    }

    $UserPath = [Environment]::GetEnvironmentVariable("Path", "User")
    if ($UserPath -notlike "*$InstallBin*") {
        [Environment]::SetEnvironmentVariable("Path", "$InstallBin;$UserPath", "User")
        Write-Host "Added $InstallBin to user PATH." -ForegroundColor Green
        Write-Host "Restart your terminal for PATH changes to take effect." -ForegroundColor Yellow
    } else {
        Write-Host "$InstallBin already in PATH." -ForegroundColor Gray
    }

    Write-Host "Manifast installed successfully!" -ForegroundColor Green
    exit 0
}

if ($Command -eq "uninstall") {
    $InstallDir = "$env:LOCALAPPDATA\Manifast"
    $InstallBin = "$InstallDir\bin"

    Write-Host "Uninstalling Manifast..." -ForegroundColor Cyan
    $UserPath = [Environment]::GetEnvironmentVariable("Path", "User")
    if ($UserPath -like "*$InstallBin*") {
        $NewPath = ($UserPath -split ";" | Where-Object { $_ -ne $InstallBin }) -join ";"
        [Environment]::SetEnvironmentVariable("Path", $NewPath, "User")
        Write-Host "Removed $InstallBin from user PATH." -ForegroundColor Green
    }

    if (Test-Path $InstallDir) {
        Remove-Item $InstallDir -Recurse -Force -ErrorAction SilentlyContinue
    }

    Write-Host "Manifast uninstalled successfully." -ForegroundColor Green
    exit 0
}

if ($Command -eq "build-wasm") {
    Write-Host "Building WebAssembly..." -ForegroundColor Cyan
    $WasmBuildDir = "build-wasm"

    if (Test-Path $WasmBuildDir) {
        Remove-Item $WasmBuildDir -Recurse -Force -ErrorAction SilentlyContinue
    }

    $EmsdkEnv = Find-EmsdkEnv
    # Prefer Ninja from MSYS2/winget when available
    $msysPrefix = Find-Msys2Ucrt
    if ($msysPrefix) {
        $env:PATH = "$msysPrefix\bin;" + $env:PATH
    }
    $gen = "Ninja"
    if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) {
        $gen = "MinGW Makefiles"
    }

    if ($EmsdkEnv) {
        Write-Host "Activating Emscripten ($EmsdkEnv)..." -ForegroundColor Gray
        cmd /c "call `"$EmsdkEnv`" > NUL && emcmake cmake -G `"$gen`" -S src/wasm -B $WasmBuildDir && cmake --build $WasmBuildDir --target manifast"
    } elseif (Get-Command emcmake -ErrorAction SilentlyContinue) {
        emcmake cmake -G $gen -S src/wasm -B $WasmBuildDir
        if ($LASTEXITCODE -eq 0) {
            cmake --build $WasmBuildDir --target manifast
        }
    } else {
        Write-Host "Error: Emscripten not found. Set EMSDK (e.g. C:\emsdk) or put emcmake on PATH." -ForegroundColor Red
        exit 1
    }

    if ($LASTEXITCODE -eq 0) {
        Write-Host "WASM Build Success!" -ForegroundColor Green
        Write-Host "Playground assets: docs\manifast.js, docs\manifast.wasm, docs\index.html" -ForegroundColor Gray
    } else {
        Write-Host "WASM Build Failed!" -ForegroundColor Red
    }
    exit $LASTEXITCODE
}
