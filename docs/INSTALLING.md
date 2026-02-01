# Installing Dependencies with MSYS2 (Fast Build)

If you want to avoid long download and compile times with `vcpkg`, you can use **MSYS2** to install pre-compiled binaries for LLVM, GTest, and fmt. This method is used by the `.\manifast.ps1 build --fast` command.

## 1. Install MSYS2
Download and install MSYS2 from [msys2.org](https://www.msys2.org/). Follow the default installation instructions.

## 2. Update MSYS2
Open the **MSYS2 UCRT64** terminal and run:
```bash
pacman -Syu
```
*(You might need to restart the terminal after the first update).*

## 3. Install Compiler and Dependencies
In the same **MSYS2 UCRT64** terminal, run the following command to install the required toolchain and libraries:

```bash
pacman -S mingw-w64-ucrt-x86_64-gcc \
          mingw-w64-ucrt-x86_64-cmake \
          mingw-w64-ucrt-x86_64-ninja \
          mingw-w64-ucrt-x86_64-llvm \
          mingw-w64-ucrt-x86_64-gtest \
          mingw-w64-ucrt-x86_64-fmt
```

## 4. Build Manifast
Once the dependencies are installed, you can build the project using the "Fast" mode in PowerShell:

```powershell
.\manifast.ps1 build --fast
```

The script will automatically detect your MSYS2 installation at `D:\Program\msys64\ucrt64` (or common defaults) and link against the system libraries.

---

### Why use this instead of vcpkg?
- **Speed**: Pacman downloads pre-compiled binaries. Vcpkg often compiles LLVM from source, which can take over an hour.
- **Consistency**: Uses the same MinGW/UCRT environment for both the compiler and the libraries, avoiding ABI mismatches.
