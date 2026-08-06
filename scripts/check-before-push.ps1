#Requires -Version 5.1
<#
.SYNOPSIS
  Quality gate before push: full tests must pass, then rebuild WASM into docs/.

.DESCRIPTION
  1. Ensure native build exists (rebuild if missing)
  2. Run mifast test + ctest — fail hard if any fail
  3. Rebuild WASM (requires Emscripten) and copy assets to docs/
  4. Fail if docs/ WASM outputs are missing or if the tree is dirty after rebuild
     (forces you to commit updated playground assets before push)

.PARAMETER SkipWasm
  Skip WASM rebuild (emergency only). Prefer fixing EMSDK instead.

.PARAMETER SkipTests
  Not recommended. Skips language/unit tests.
#>
param(
    [switch]$SkipWasm,
    [switch]$SkipTests
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if (-not (Test-Path (Join-Path $Root "CMakeLists.txt"))) {
    $Root = Get-Location
}
Set-Location $Root

function Write-Step($m) { Write-Host "==> $m" -ForegroundColor Cyan }
function Fail($m) { Write-Host "FAIL: $m" -ForegroundColor Red; exit 1 }

function Ensure-UcrtPath {
    $candidates = @()
    if ($env:MSYS2_ROOT) { $candidates += (Join-Path $env:MSYS2_ROOT "ucrt64\bin") }
    $candidates += @("C:\msys64\ucrt64\bin", "D:\msys64\ucrt64\bin")
    foreach ($p in $candidates) {
        if (Test-Path $p) {
            if ($env:PATH -notlike "*$p*") { $env:PATH = "$p;" + $env:PATH }
            return
        }
    }
}

function Find-Mifast {
    $candidates = @(
        "build\bin\mifast.exe",
        "build\bin\mifast"
    )
    foreach ($c in $candidates) {
        if (Test-Path $c) { return (Resolve-Path $c).Path }
    }
    return $null
}

Ensure-UcrtPath

# --- Build if needed ---
$mifast = Find-Mifast
if (-not $mifast) {
    Write-Step "No mifast binary — building..."
    if (Test-Path ".\manifast.ps1") {
        & ".\manifast.ps1" build --fast
        if ($LASTEXITCODE -ne 0) { Fail "native build failed" }
    } else {
        Fail "mifast not found and manifast.ps1 missing"
    }
    $mifast = Find-Mifast
    if (-not $mifast) { Fail "mifast still missing after build" }
}

# --- Tests ---
if (-not $SkipTests) {
    Write-Step "Running language test suite (mifast test)..."
    & $mifast test
    if ($LASTEXITCODE -ne 0) { Fail "mifast test failed (exit $LASTEXITCODE)" }

    Write-Step "Running CTest unit tests..."
    if (-not (Get-Command ctest -ErrorAction SilentlyContinue)) {
        Fail "ctest not on PATH"
    }
    if (-not (Test-Path "build")) { Fail "build/ missing for ctest" }
    ctest --test-dir build --output-on-failure
    if ($LASTEXITCODE -ne 0) { Fail "ctest failed (exit $LASTEXITCODE)" }
    Write-Host "  Tests OK" -ForegroundColor Green
} else {
    Write-Host "  WARN: tests skipped (-SkipTests)" -ForegroundColor Yellow
}

# --- WASM ---
if ($SkipWasm) {
    Write-Host "  WARN: WASM rebuild skipped (-SkipWasm)" -ForegroundColor Yellow
} else {
    Write-Step "Rebuilding WASM playground assets..."
    # Prefer EMSDK env, then common install path
    $emsdkEnv = $null
    if ($env:EMSDK -and (Test-Path (Join-Path $env:EMSDK "emsdk_env.bat"))) {
        $emsdkEnv = Join-Path $env:EMSDK "emsdk_env.bat"
    } elseif (Test-Path "C:\emsdk\emsdk_env.bat") {
        $emsdkEnv = "C:\emsdk\emsdk_env.bat"
        $env:EMSDK = "C:\emsdk"
    }

    $hasEmcmake = [bool](Get-Command emcmake -ErrorAction SilentlyContinue)
    if (-not $hasEmcmake -and -not $emsdkEnv) {
        Fail "Emscripten not found. Install EMSDK or set EMSDK. Use -SkipWasm only in emergencies."
    }

    if ($emsdkEnv -and -not $hasEmcmake) {
        # Run build inside activated EMSDK shell
        $cmd = @"
call "$emsdkEnv" >nul 2>&1
if errorlevel 1 exit /b 1
cd /d "$Root"
powershell -NoProfile -ExecutionPolicy Bypass -File ".\manifast.ps1" build-wasm
exit /b %ERRORLEVEL%
"@
        $tmp = Join-Path $env:TEMP "manifast-wasm-build.cmd"
        Set-Content -Path $tmp -Value $cmd -Encoding ASCII
        cmd /c $tmp
        if ($LASTEXITCODE -ne 0) { Fail "build-wasm failed (exit $LASTEXITCODE)" }
    } else {
        & ".\manifast.ps1" build-wasm
        if ($LASTEXITCODE -ne 0) { Fail "build-wasm failed (exit $LASTEXITCODE)" }
    }

    foreach ($asset in @("docs\manifast.js", "docs\manifast.wasm", "docs\index.html")) {
        if (-not (Test-Path $asset)) { Fail "missing required web asset after WASM build: $asset" }
    }

    # If WASM rebuild changed tracked files, require a commit before push
    git rev-parse --is-inside-work-tree 2>$null | Out-Null
    if ($LASTEXITCODE -eq 0) {
        $dirty = git status --porcelain -- docs/ src/wasm/ 2>$null
        if ($dirty) {
            Write-Host $dirty
            Fail "WASM/web assets changed. Commit docs/ (and src/wasm if needed), then push again."
        }
    }
    Write-Host "  WASM/docs OK" -ForegroundColor Green
}

Write-Host ""
Write-Host "Pre-push checks passed." -ForegroundColor Green
exit 0
