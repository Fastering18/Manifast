# Manifast Programming Language
https://github.com/Fastering18/Manifast  
Fast, scriptable programming language with Indonesian-based syntax, powered by LLVM.

## Syntax Samples
```manifast
-- Functions
fungsi hitung(x, y)
    kembali x * y + 10
tutup

-- Variables and Control Flow
lokal a = 5
jika a < 10 maka
    untuk i = 1 ke 5 lakukan
        a += i
    tutup
tutup

-- Tables/Objects
lokal user = { nama: "Manifast", versi: 0.1 }
```

[Full Grammar Definition (BNF)](docs/GRAMMAR.md)

## Welcome Contributors!
We are open to anyone who wants to contribute to this project. Whether it's reporting bugs, suggesting features, or submitting pull requests, your help is appreciated!

## Todo list
- [x] Lexer (Tokens, Indonesian Keywords, Lua-style Comments)
- [x] Parser (Hand-written Recursive Descent, AST)
- [x] Optional Semicolons
- [/] LLVM Code Generation (Foundation, IR Extraction)
- [ ] Variables & Scoping
- [ ] Function Codegen
- [ ] Control Flow (jika, sementara, untuk)
- [ ] Try-Catch Implementation (coba-tangkap)
- [ ] Tables & Arrays (Object-oriented features)
- [ ] Memory Management (C++ RAII/Ownership)
- [ ] Standard Library (print, io, math)
- [ ] JIT Compiler Pipeline
- [ ] IDE Support (LSP)

## How to compile & run?
### Requirements
- [vcpkg](https://vcpkg.io/) for dependency management
- [CMake](https://cmake.org/) (version 3.25+)
- [Ninja](https://ninja-build.org/) or any C++ build tool
- [LLVM](https://llvm.org/) (Recommended 18+)

### Clone the project
```powershell
git clone https://github.com/Fastering18/Manifast
cd Manifast
```

### Setup Dependencies
Make sure `vcpkg` is installed and in your PATH.
```powershell
vcpkg install
```

**Alternative (Recommended for Windows Speed):**
For installing dependencies without vcpkg: [follow these steps in docs/INSTALLING.md](docs/INSTALLING.md)


### LLVM Recommendation
We recommend using **LLVM 18+**.
- **Default**: The build system will automatically download and build the latest available LLVM via `vcpkg` (can be slow).
- **Fast Build**: Install LLVM 18+ manually and use the `--fast` flag to skip the vcpkg build.

### Build & Run
We provide a unified helper script: `manifast.ps1` (Windows) or `manifast.sh` (Linux/macOS).

**Standard Build (Clone & Go):**
```powershell
.\manifast.ps1 build
```

**Fast Build (Uses System LLVM):**
```powershell
.\manifast.ps1 build --fast
```

**Run Tests:**
```powershell
.\manifast.ps1 test
```

**Clean:**
```powershell
.\manifast.ps1 clean
```

---

Join our conversation here for development: [Discord Server](https://discord.gg/8vdZsBBGRG)
