#Requires -Version 5.1
# Point this repo at versioned hooks under .githooks/
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $Root

if (-not (Test-Path ".githooks\pre-push")) {
    Write-Error "Missing .githooks/pre-push"
    exit 1
}

git config core.hooksPath .githooks
Write-Host "Installed git hooks: core.hooksPath=.githooks" -ForegroundColor Green
Write-Host "Pre-push will run scripts/check-before-push.ps1 (tests + WASM)."
Write-Host "Emergency bypass: `$env:SKIP_PUSH_CHECKS=1; git push"
