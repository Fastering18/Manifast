# Manifast One-Liner Installer for Windows
# Usage: iwr -useb https://manifast.dev/install.ps1 | iex

$repo = "Fastering18/Manifast"
$platform = "windows"

Write-Host "--- Manifast Installer ---" -ForegroundColor Cyan

# Get latest release tag
try {
    $release = Invoke-RestMethod -Uri "https://api.github.com/repos/$repo/releases/latest"
    $tag = $release.tag_name
} catch {
    Write-Error "Could not find latest release."
    return
}

$filename = "manifast-$platform-x64.zip"
$url = "https://github.com/$repo/releases/download/$tag/$filename"

Write-Host "Downloading $filename ($tag)..."
$tempZip = Join-Path $env:TEMP "manifast.zip"
Invoke-WebRequest -Uri $url -OutFile $tempZip

Write-Host "Extracting..."
$tempDir = Join-Path $env:TEMP "manifast_install"
if (Test-Path $tempDir) { Remove-Item -Path $tempDir -Recurse -Force }
Expand-Archive -Path $tempZip -DestinationPath $tempDir -Force

# Install to %USERPROFILE%\.manifast\bin
$installDir = Join-Path $env:USERPROFILE ".manifast\bin"
if (!(Test-Path $installDir)) { New-Item -ItemType Directory -Path $installDir -Force }

Write-Host "Installing to $installDir..."
Copy-Item -Path "$tempDir\bin\mifast*" -Destination $installDir -Force

# Add to PATH for current session if not already there
$path = [Environment]::GetEnvironmentVariable("Path", "User")
if ($path -notlike "*$installDir*") {
    Write-Host "Adding to User PATH..."
    [Environment]::SetEnvironmentVariable("Path", "$path;$installDir", "User")
    $env:Path += ";$installDir"
}

# Cleanup
Remove-Item $tempZip -Force
Remove-Item $tempDir -Recurse -Force

Write-Host "Success! Manifast installed to $installDir" -ForegroundColor Green
Write-Host "Please restart your terminal or run: `$env:Path = [System.Environment]::GetEnvironmentVariable('Path','User')`"
Write-Host "Try running: mifast version"
