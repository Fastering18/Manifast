#!/usr/bin/env bash
# Non-interactive bootstrap for Manifast on Linux (apt or dnf).
set -euo pipefail

echo "==> Manifast Linux bootstrap"

if command -v apt-get >/dev/null 2>&1; then
  echo "==> Using apt"
  sudo apt-get update
  # LLVM 18 when available; fall back to default llvm-dev
  sudo apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    g++ \
    pkg-config \
    libfmt-dev \
    libgtest-dev \
    libcurl4-openssl-dev \
    libedit-dev \
    wget \
    ca-certificates || true

  if apt-cache show llvm-18-dev >/dev/null 2>&1; then
    sudo apt-get install -y llvm-18-dev
    echo "  OK: llvm-18-dev"
    echo "  Configure with: -DLLVM_DIR=/usr/lib/llvm-18/lib/cmake/llvm"
  else
    sudo apt-get install -y llvm-dev
    echo "  OK: llvm-dev (default distro version)"
  fi
elif command -v dnf >/dev/null 2>&1; then
  echo "==> Using dnf"
  sudo dnf install -y \
    gcc-c++ \
    cmake \
    ninja-build \
    fmt-devel \
    gtest-devel \
    llvm-devel \
    libcurl-devel \
    libedit-devel
else
  echo "ERROR: Unsupported package manager. Install cmake, ninja, g++, llvm-dev, libfmt-dev, libgtest-dev manually."
  exit 1
fi

echo ""
echo "Bootstrap complete."
echo "Next:"
echo "  ./manifast.sh build --fast"
echo "  # or: cmake --preset linux-system && cmake --build build"
