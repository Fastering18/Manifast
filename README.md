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

## Performance

Manifast uses an in-process JIT compiler via LLVM. Benchmark results on Windows (AMD Ryzen 7, 8 cores):

| Category | Min | Avg | Max |
|----------|-----|-----|-----|
| Control Flow | 5.44ms | 7.23ms | 9.03ms |
| Literals | 3.68ms | 4.28ms | 5.89ms |
| Scoping | 3.38ms | 3.38ms | 3.38ms |
| OS/IO | 3.22ms | 3.22ms | 3.22ms |

> **Note**: LLVM JIT has inherent per-invocation overhead (~3-5ms). For sub-millisecond performance, consider AOT compilation (planned feature).

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
- [x] JIT Compiler Pipeline
- [ ] Emscripten/WebAssembly Support
- [ ] Embedding Language Support (Web demo)
- [ ] IDE Support (LSP)
- [ ] Self compilation
- [ ] System Embedding Language (Game engine)
- [ ] AOT Compilation (for sub-millisecond startup)

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
