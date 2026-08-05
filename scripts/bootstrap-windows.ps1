#Requires -Version 5.1
<#
.SYNOPSIS
  Non-GUI bootstrap for Manifast on Windows (winget + optional MSYS2).

.DESCRIPTION
  Installs build tools needed to compile Manifast natively on Windows.
  Default: full stack via MSYS2 UCRT64 (recommended for LLVM JIT/AOT).
  Use -VmOnly for a lighter toolchain without MSYS2/LLVM.

.EXAMPLE
  .\scripts\bootstrap-windows.ps1
  .\scripts\bootstrap-windows.ps1 -VmOnly
#>
param(
    [switch]$VmOnly,
    [switch]$SkipMsys2Packages
)

$ErrorActionPreference = "Stop"

function Write-Step($msg) { Write-Host "==> $msg" -ForegroundColor Cyan }
function Write-Ok($msg)   { Write-Host "  OK: $msg" -ForegroundColor Green }
function Write-Warn($msg) { Write-Host "  WARN: $msg" -ForegroundColor Yellow }

function Test-Command($name) {
    return [bool](Get-Command $name -ErrorAction SilentlyContinue)
}

function Install-WingetPackage {
    param(
        [Parameter(Mandatory)][string]$Id,
        [string]$Name = $Id
    )
    Write-Step "Installing $Name ($Id) via winget..."
    $args = @(
        "install", "--id", $Id, "-e",
        "--accept-package-agreements",
        "--accept-source-agreements",
        "--disable-interactivity"
    )
    & winget @args
    if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne -1978335189) {
        # -1978335189 = already installed (winget)
        Write-Warn "winget returned exit code $LASTEXITCODE for $Id (may already be installed)"
    } else {
        Write-Ok "$Name ready"
    }
}

# --- winget ---
if (-not (Test-Command "winget")) {
    Write-Host "ERROR: winget not found. Install App Installer from Microsoft Store, then re-run." -ForegroundColor Red
    exit 1
}

# Refresh PATH helper for current session after installs
function Refresh-Path {
    $machine = [Environment]::GetEnvironmentVariable("Path", "Machine")
    $user = [Environment]::GetEnvironmentVariable("Path", "User")
    $env:Path = "$machine;$user"
}

# --- Core tools ---
if (-not (Test-Command "cmake")) {
    Install-WingetPackage -Id "Kitware.CMake" -Name "CMake"
    Refresh-Path
} else {
    Write-Ok "cmake already present: $((Get-Command cmake).Source)"
}

if (-not (Test-Command "ninja") -and -not (Test-Command "ninja.exe")) {
    Install-WingetPackage -Id "Ninja-build.Ninja" -Name "Ninja"
    Refresh-Path
} else {
    Write-Ok "ninja already present"
}

if (-not (Test-Command "git")) {
    Install-WingetPackage -Id "Git.Git" -Name "Git"
    Refresh-Path
}

if ($VmOnly) {
    Write-Step "VM-only mode: skipping MSYS2 / LLVM"
    Write-Host ""
    Write-Host "Next steps:" -ForegroundColor Cyan
    Write-Host "  1. Install a C++ compiler (MSVC Build Tools or MinGW on PATH)"
    Write-Host "  2. Optionally set VCPKG_ROOT and run: vcpkg install"
    Write-Host "  3. cmake --preset no-llvm"
    Write-Host "  4. cmake --build build"
    Write-Host "  Or: .\manifast.ps1 build --fast   (if system libs available)"
    exit 0
}

# --- MSYS2 full stack ---
function Find-Msys2Root {
    if ($env:MSYS2_ROOT -and (Test-Path $env:MSYS2_ROOT)) {
        return $env:MSYS2_ROOT
    }
    foreach ($candidate in @(
        "C:\msys64",
        "D:\msys64",
        "C:\tools\msys64",
        "$env:SystemDrive\msys64"
    )) {
        if (Test-Path $candidate) { return $candidate }
    }
    # winget sometimes installs under Program Files or LocalAppData
    $wingetLinks = @(
        "$env:LOCALAPPDATA\Microsoft\WinGet\Links",
        "$env:ProgramFiles\msys64",
        "${env:ProgramFiles(x86)}\msys64"
    )
    foreach ($c in $wingetLinks) {
        if (Test-Path $c) { return $c }
    }
    return $null
}

$msysRoot = Find-Msys2Root
if (-not $msysRoot) {
    Write-Step "Installing MSYS2 via winget..."
    Install-WingetPackage -Id "MSYS2.MSYS2" -Name "MSYS2"
    Refresh-Path
    Start-Sleep -Seconds 2
    $msysRoot = Find-Msys2Root
}

if (-not $msysRoot) {
    Write-Host "ERROR: MSYS2 installed but root not found. Set MSYS2_ROOT and re-run." -ForegroundColor Red
    exit 1
}

Write-Ok "MSYS2 root: $msysRoot"
$env:MSYS2_ROOT = $msysRoot

$bash = Join-Path $msysRoot "usr\bin\bash.exe"
if (-not (Test-Path $bash)) {
    Write-Host "ERROR: MSYS2 bash not found at $bash" -ForegroundColor Red
    exit 1
}

if (-not $SkipMsys2Packages) {
    Write-Step "Installing UCRT64 toolchain + LLVM + deps (pacman, non-interactive)..."
    $packages = @(
        "mingw-w64-ucrt-x86_64-toolchain",
        "mingw-w64-ucrt-x86_64-cmake",
        "mingw-w64-ucrt-x86_64-ninja",
        "mingw-w64-ucrt-x86_64-llvm",
        "mingw-w64-ucrt-x86_64-fmt",
        "mingw-w64-ucrt-x86_64-gtest"
    )
    # asmjit is optional in MSYS2; ignore failure
    $pkgList = $packages -join " "
    $pacmanCmd = "pacman -Syu --noconfirm && pacman -S --needed --noconfirm $pkgList"
    & $bash -lc $pacmanCmd
    if ($LASTEXITCODE -ne 0) {
        Write-Warn "pacman exited with $LASTEXITCODE — try opening MSYS2 UCRT64 and re-running packages manually"
    } else {
        Write-Ok "MSYS2 packages installed"
    }

    # Optional asmjit
    & $bash -lc "pacman -S --needed --noconfirm mingw-w64-ucrt-x86_64-asmjit" 2>$null
}

$ucrtBin = Join-Path $msysRoot "ucrt64\bin"
if (Test-Path $ucrtBin) {
    if ($env:Path -notlike "*$ucrtBin*") {
        $env:Path = "$ucrtBin;$env:Path"
        Write-Ok "Prepended $ucrtBin to session PATH"
    }
}

Write-Host ""
Write-Host "Bootstrap complete." -ForegroundColor Green
Write-Host ""
Write-Host "Open a new terminal (or keep this session), then:" -ForegroundColor Cyan
Write-Host "  `$env:MSYS2_ROOT = '$msysRoot'"
Write-Host "  `$env:Path = '$ucrtBin;' + `$env:Path"
Write-Host "  .\manifast.ps1 build --fast"
Write-Host ""
Write-Host "Or with CMake presets (from UCRT64 environment):" -ForegroundColor Cyan
Write-Host "  cmake --preset windows-msys2"
Write-Host "  cmake --build build"
Write-Host ""
Write-Host "Tip: permanently add $ucrtBin to User PATH for convenience." -ForegroundColor Gray
