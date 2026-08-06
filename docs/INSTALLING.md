# Installing build dependencies

Manifast is a standard **CMake C++20** project. You can develop and ship **native Windows** builds without WSL.

---

## Windows (recommended, non-GUI)

From PowerShell in the repository root:

```powershell
.\scripts\bootstrap-windows.ps1
# or
.\manifast.ps1 bootstrap
```

This uses **winget** for CMake/Ninja when needed and **MSYS2 UCRT64** for:

- MinGW toolchain (`g++`)
- Prebuilt **LLVM** (much faster than building LLVM via vcpkg)
- `fmt`, `gtest`

Then:

```powershell
$env:MSYS2_ROOT = "C:\msys64"   # if not already set
$env:Path = "C:\msys64\ucrt64\bin;" + $env:Path
.\manifast.ps1 build --fast
.\manifast.ps1 install-hooks    # enable pre-push: tests + WASM
```

### VM-only (no LLVM)

```powershell
.\scripts\bootstrap-windows.ps1 -VmOnly
cmake --preset no-llvm
cmake --build build
```

### Manual MSYS2

1. `winget install MSYS2.MSYS2` (or install from [msys2.org](https://www.msys2.org/))
2. In MSYS2 bash:

```bash
pacman -Syu --noconfirm
pacman -S --needed --noconfirm \
  mingw-w64-ucrt-x86_64-toolchain \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-llvm \
  mingw-w64-ucrt-x86_64-fmt \
  mingw-w64-ucrt-x86_64-gtest
```

3. Put `C:\msys64\ucrt64\bin` on `PATH`, or set `MSYS2_ROOT`.

### WebAssembly (playground)

Requires [Emscripten](https://emscripten.org/) (EMSDK). Example:

```powershell
# if emsdk lives at C:\emsdk
$env:EMSDK = "C:\emsdk"
.\manifast.ps1 build-wasm
```

Outputs land in `docs/` (`manifast.js`, `manifast.wasm`, `index.html`).

---

## Linux

```bash
./scripts/bootstrap-linux.sh
# or
./manifast.sh bootstrap

./manifast.sh build --fast
./scripts/install-hooks.sh
```

Ubuntu/Debian installs cmake, ninja, g++, llvm (18 when available), libfmt-dev, and libgtest-dev.

---

## vcpkg (optional)

`vcpkg.json` depends on `fmt`, `asmjit`, `gtest`, and `argparse`. **LLVM is not a default feature** (source builds are slow).

```bash
# with VCPKG_ROOT set
vcpkg install
cmake --preset vcpkg
```

Opt-in LLVM via vcpkg:

```bash
vcpkg install --x-feature=bundled-llvm
```

Prefer system or MSYS2 LLVM with `--fast` for day-to-day work.

---

## CMake presets

| Preset | When to use |
|--------|-------------|
| `windows-msys2` | Full Windows + MSYS2 UCRT64 + LLVM |
| `windows-msvc-vm` | MSVC + vcpkg, no LLVM |
| `linux-system` | Linux distro packages |
| `no-llvm` | Portable VM-only |
| `vcpkg` | Manifest mode via `VCPKG_ROOT` |

```bash
cmake --preset no-llvm
cmake --build build
```

---

## Pre-push quality gate

Before pushing, tests must pass and the WASM playground should be rebuilt:

```powershell
.\manifast.ps1 check
```

```bash
./scripts/check-before-push.sh
```

Details: [CONTRIBUTING.md](../CONTRIBUTING.md).

---

## Why not WSL-only?

Ship and debug native Windows binaries without a second environment. WSL remains optional for Linux parity, not a requirement for Manifast on Windows.
