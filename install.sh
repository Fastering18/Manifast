#!/bin/bash
set -e

# Manifast One-Liner Installer for Linux/macOS
# Usage: curl -fsSL https://manifast.dev/install.sh | bash

REPO="Fastering18/Manifast"
PLATFORM="linux"
if [[ "$OSTYPE" == "darwin"* ]]; then
    PLATFORM="macos"
fi

echo "--- Manifast Installer ---"

# Get latest release tag
LATEST_TAG=$(curl -s https://api.github.com/repos/$REPO/releases/latest | grep '"tag_name":' | sed -E 's/.*"([^"]+)".*/\1/')

if [ -z "$LATEST_TAG" ]; then
    echo "Error: Could not find latest release."
    exit 1
fi

FILENAME="manifast-${PLATFORM}-x64.zip"
URL="https://github.com/$REPO/releases/download/$LATEST_TAG/$FILENAME"

echo "Downloading $FILENAME ($LATEST_TAG)..."
curl -L -o manifast.zip "$URL"

echo "Extracting..."
mkdir -p manifast_temp
unzip -q manifast.zip -d manifast_temp

# Install to /usr/local/bin or ~/.local/bin
INSTALL_DIR="/usr/local/bin"
if [ ! -w "$INSTALL_DIR" ]; then
    INSTALL_DIR="$HOME/.local/bin"
    mkdir -p "$INSTALL_DIR"
fi

echo "Installing to $INSTALL_DIR..."
cp manifast_temp/bin/mifast* "$INSTALL_DIR/"
chmod +x "$INSTALL_DIR"/mifast*

# Cleanup
rm manifast.zip
rm -rf manifast_temp

echo "Success! Manifast installed to $INSTALL_DIR"
echo "Try running: mifast version"
