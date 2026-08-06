#Requires -Version 5.1
<#
.SYNOPSIS
  Quality gate before push: full tests must pass, then rebuild WASM into docs/.

.DESCRIPTION
  1. Ensure native build exists (rebuild if missing)
  2. Run mifast test + ctest - fail hard if any fail
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
    $Root = (Get-Location).Path
}
Set-Location $Root

function Write-Step([string]$m) { Write-Host "==> $m" -ForegroundColor Cyan }
function Fail([string]$m) { Write-Host "FAIL: $m" -ForegroundColor Red; exit 1 }

function Ensure-UcrtPath {
    $candidates = @()
    if ($env:MSYS2_ROOT) { $candidates += (Join-Path $env:MSYS2_ROOT "ucrt64\bin") }
    $candidates += @("C:\msys64\ucrt64\bin", "D:\msys64\ucrt64\bin")
    foreach ($p in $candidates) {
        if (Test-Path $p) {
            # Always prepend so git-hook / mixed MinGW PATHs do not shadow UCRT DLLs
            $env:PATH = "$p;" + $env:PATH
            $env:MSYS2_ROOT = (Split-Path (Split-Path $p))
            return $p
        }
    }
    return $null
}

function Find-Mifast {
    foreach ($c in @("build\bin\mifast.exe", "build\bin\mifast")) {
        if (Test-Path $c) { return (Resolve-Path $c).Path }
    }
    return $null
}

$ucrtBin = Ensure-UcrtPath
if (-not $ucrtBin) {
    Write-Host "WARN: MSYS2 UCRT64 bin not found; mifast may fail to load DLLs" -ForegroundColor Yellow
}

# --- Build if needed ---
$mifast = Find-Mifast
if (-not $mifast) {
    Write-Step "No mifast binary - building..."
    if (Test-Path (Join-Path $Root "manifast.ps1")) {
        & (Join-Path $Root "manifast.ps1") build --fast
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
    if ($ucrtBin) { $env:PATH = "$ucrtBin;" + $env:PATH }
    & $mifast test
    if ($LASTEXITCODE -ne 0) { Fail ("mifast test failed (exit " + $LASTEXITCODE + ")") }

    Write-Step "Running CTest unit tests..."
    if (-not (Get-Command ctest -ErrorAction SilentlyContinue)) {
        Fail "ctest not on PATH"
    }
    if (-not (Test-Path "build")) { Fail "build/ missing for ctest" }
    ctest --test-dir build --output-on-failure
    if ($LASTEXITCODE -ne 0) { Fail ("ctest failed (exit " + $LASTEXITCODE + ")") }
    Write-Host "  Tests OK" -ForegroundColor Green
} else {
    Write-Host "  WARN: tests skipped (-SkipTests)" -ForegroundColor Yellow
}

# --- WASM ---
if ($SkipWasm) {
    Write-Host "  WARN: WASM rebuild skipped (-SkipWasm)" -ForegroundColor Yellow
} else {
    Write-Step "Rebuilding WASM playground assets..."

    # Normalize MSYS/Git-Bash EMSDK paths like /c/emsdk -> C:\emsdk
    if ($env:EMSDK -match '^/([a-zA-Z])/(.*)$') {
        $env:EMSDK = ($Matches[1].ToUpper() + ":\" + ($Matches[2] -replace '/', '\'))
    }

    $emsdkEnv = $null
    if ($env:EMSDK) {
        $candidate = Join-Path $env:EMSDK "emsdk_env.bat"
        if (Test-Path $candidate) { $emsdkEnv = $candidate }
    }
    if (-not $emsdkEnv) {
        foreach ($root in @("C:\emsdk", "D:\emsdk", (Join-Path $env:USERPROFILE "emsdk"))) {
            $candidate = Join-Path $root "emsdk_env.bat"
            if (Test-Path $candidate) {
                $emsdkEnv = $candidate
                $env:EMSDK = $root
                break
            }
        }
    }

    $hasEmcmake = [bool](Get-Command emcmake -ErrorAction SilentlyContinue)
    if (-not $hasEmcmake -and -not $emsdkEnv) {
        Fail "Emscripten not found. Install EMSDK or set EMSDK. Use -SkipWasm only in emergencies."
    }

    $ps1 = Join-Path $Root "manifast.ps1"
    if ($emsdkEnv -and -not $hasEmcmake) {
        # Write a tiny cmd file so PowerShell never parses cmd metacharacters
        $tmpCmd = Join-Path $env:TEMP "manifast-prepush-wasm.cmd"
        $lines = @(
            "@echo off"
            ('call "' + $emsdkEnv + '" >nul 2>&1')
            ("if errorlevel 1 exit /b 1")
            ('cd /d "' + $Root + '"')
            ('powershell -NoProfile -ExecutionPolicy Bypass -File "' + $ps1 + '" build-wasm')
            "exit /b %ERRORLEVEL%"
        )
        Set-Content -Path $tmpCmd -Value $lines -Encoding ASCII
        cmd.exe /c $tmpCmd
        if ($LASTEXITCODE -ne 0) { Fail ("build-wasm failed (exit " + $LASTEXITCODE + ")") }
    } else {
        & $ps1 build-wasm
        if ($LASTEXITCODE -ne 0) { Fail ("build-wasm failed (exit " + $LASTEXITCODE + ")") }
    }

    foreach ($asset in @("docs\manifast.js", "docs\manifast.wasm", "docs\index.html")) {
        if (-not (Test-Path $asset)) { Fail ("missing required web asset after WASM build: " + $asset) }
    }

    # If WASM rebuild changed tracked files, require a commit before push
    $null = git rev-parse --is-inside-work-tree 2>$null
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
