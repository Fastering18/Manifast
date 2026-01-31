# Manifast Programming Language
https://github.com/Fastering18/Manifast  
Fast, scriptable programming language with Indonesian-based syntax, powered by LLVM.

## Welcome Contributors!
We are open to anyone who wants to contribute to this project. Whether it's reporting bugs, suggesting features, or submitting pull requests, your help is appreciated!

Join our conversation here to get help or discuss development: [Discord Server](https://discord.gg/8vdZsBBGRG)

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

### Build & Run
You can use the provided helper script:
```powershell
.\build.ps1 -Clean -Run
```

Or manually:
```powershell
cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
cmake --build build
.\build\bin\manifast tests/Manifast/test.mnf
```

---
Powered by LLVM. Made with ❤️ for the Indonesian Developer Community.
