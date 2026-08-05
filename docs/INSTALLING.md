# Installing Build Dependencies

Manifast is a standard CMake C++20 project. You do **not** need WSL to develop on Windows.

## Windows (recommended — non-GUI)

From PowerShell in the repo root:

```powershell
.\scripts\bootstrap-windows.ps1
# or:
.\manifast.ps1 bootstrap
```

This uses **winget** to install CMake/Ninja (if missing) and **MSYS2 UCRT64** packages for:

- MinGW toolchain (`g++`)
- LLVM (prebuilt — much faster than compiling via vcpkg)
- fmt, gtest

Then build:

```powershell
$env:MSYS2_ROOT = "C:\msys64"   # if not already set
$env:Path = "C:\msys64\ucrt64\bin;" + $env:Path
.\manifast.ps1 build --fast
```

### VM-only (no LLVM)

If you only need the bytecode VM (faster bootstrap, no JIT/AOT):

```powershell
.\scripts\bootstrap-windows.ps1 -VmOnly
cmake --preset no-llvm
cmake --build build
```

### Manual MSYS2 (optional)

1. Install [MSYS2](https://www.msys2.org/) or `winget install MSYS2.MSYS2`
2. In a shell with MSYS2 `bash`:

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

3. Put `C:\msys64\ucrt64\bin` on PATH (or set `MSYS2_ROOT`).

---

## Linux

```bash
./scripts/bootstrap-linux.sh
# or:
./manifast.sh bootstrap

./manifast.sh build --fast
```

On Ubuntu/Debian this installs cmake, ninja, g++, llvm (18 when available), libfmt-dev, libgtest-dev.

---

## vcpkg (optional)

`vcpkg.json` lists: `fmt`, `asmjit`, `gtest`, `argparse`. LLVM is **not** a default feature (building it from source is slow).

```bash
# after setting VCPKG_ROOT
vcpkg install
cmake --preset vcpkg   # or pass -DCMAKE_TOOLCHAIN_FILE=...
```

To force LLVM via vcpkg (slow):

```bash
vcpkg install --x-feature=bundled-llvm
```

Prefer system/MSYS2 LLVM with `--fast` for day-to-day work.

---

## CMake presets

| Preset | Use when |
|--------|----------|
| `windows-msys2` | Full Windows + MSYS2 UCRT64 |
| `windows-msvc-vm` | MSVC + vcpkg, no LLVM |
| `linux-system` | Linux with distro packages |
| `no-llvm` | Portable VM-only |
| `vcpkg` | Manifest mode via `VCPKG_ROOT` |

```bash
cmake --preset no-llvm
cmake --build build
```

---

## Why not WSL-only?

Ship and debug native Windows binaries without a second environment. WSL is fine for optional Linux parity testing, not required for Manifast development on Windows.
