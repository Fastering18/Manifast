# Manifast Programming Language
[![Build and Test](https://github.com/Fastering18/Manifast/actions/workflows/ci.yml/badge.svg)](https://github.com/Fastering18/Manifast/actions/workflows/ci.yml)
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

## Performance

Manifast features a **Tier-0 Bytecode VM** designed for ultra-low latency embedding (e.g., Game Engines, Configuration, Web).

| Metric | Time | Notes |
|--------|------|-------|
| **VM Startup** | **~0.05 ms** (50µs) | Full Pipeline (Lex/Parse/Compile/Run) |
| **Execution** | **<0.01 ms** (10µs) | VM Loop Only (Est.) |
| **Memory** | **<500 KB** | Core Library Size |

## Todo list
- [x] Lexer (Tokens, Indonesian Keywords, Lua-style Comments)
- [x] Parser (Hand-written Recursive Descent, AST)
- [x] Optional Semicolons
- [x] LLVM Code Generation (Foundation, IR Extraction)
- [x] Variables & Scoping
- [x] Function Codegen
- [x] Control Flow (jika, kalau, kecuali, selama, untuk)
- [x] Try-Catch Implementation (coba-tangkap)
- [x] Tables & Arrays (Object-oriented features)
- [x] Memory Management (C++ RAII/Ownership)
- [x] Standard Library (print, println, printfmt, input)
- [x] JIT Compiler Pipeline (Tier 1)
- [x] **Bytecode VM (Tier 0)** - Ultra-fast startup
- [x] Emscripten/WebAssembly Support
- [x] Embedding Language Support ([Live Playground](https://fastering18.github.io/Manifast/))
- [ ] IDE Support (LSP)
- [ ] Self compilation
- [x] System Embedding Language (Game engine ready)

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
