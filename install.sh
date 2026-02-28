#!/bin/bash
set -e

# Manifast Installer for Linux/macOS/Android(Termux)
# Usage: curl -fsSL https://raw.githubusercontent.com/Fastering18/Manifast/master/install.sh | bash

REPO="Fastering18/Manifast"

echo "--- Manifast Installer ---"

# Detect platform
if [ -d "/data/data/com.termux" ] || [ -n "$TERMUX_VERSION" ]; then
    PLATFORM="android"
    echo "Detected: Android (Termux)"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    PLATFORM="macos"
    echo "Detected: macOS"
else
    PLATFORM="linux"
    echo "Detected: Linux"
fi

# Get latest release tag
LATEST_TAG=$(curl -s https://api.github.com/repos/$REPO/releases/latest | grep '"tag_name":' | sed -E 's/.*"([^"]+)".*/\1/')

if [ -z "$LATEST_TAG" ]; then
    echo "Error: Could not find latest release."
    exit 1
fi

# Strip 'v' prefix for filename (CPack uses version without v)
VERSION="${LATEST_TAG#v}"

# Download the tarball (CPack naming: manifast-{version}-Linux.tar.gz)
FILENAME="manifast-${VERSION}-Linux.tar.gz"
URL="https://github.com/$REPO/releases/download/$LATEST_TAG/$FILENAME"

echo "Downloading $FILENAME ($LATEST_TAG)..."
curl -L -o manifast.tar.gz "$URL"

echo "Extracting..."
mkdir -p manifast_temp
tar -xzf manifast.tar.gz -C manifast_temp --strip-components=1

# Determine install directory
if [ "$PLATFORM" = "android" ]; then
    INSTALL_DIR="$PREFIX/bin"
    mkdir -p "$INSTALL_DIR"
elif [ -w "/usr/local/bin" ]; then
    INSTALL_DIR="/usr/local/bin"
else
    INSTALL_DIR="$HOME/.local/bin"
    mkdir -p "$INSTALL_DIR"
fi

echo "Installing to $INSTALL_DIR..."
cp manifast_temp/bin/mifast* "$INSTALL_DIR/"
chmod +x "$INSTALL_DIR"/mifast*

# Cleanup
rm manifast.tar.gz
rm -rf manifast_temp

echo "Success! Manifast installed to $INSTALL_DIR"

# Check if install dir is in PATH
case ":$PATH:" in
    *":$INSTALL_DIR:"*) ;;
    *)
        echo ""
        echo "NOTE: $INSTALL_DIR is not in your PATH."
        echo "Add it with: export PATH=\"\$PATH:$INSTALL_DIR\""
        ;;
esac

echo "Try running: mifast --help"
