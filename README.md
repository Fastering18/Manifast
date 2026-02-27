# Manifast Programming Language
[![Build and Test](https://github.com/Fastering18/Manifast/actions/workflows/ci.yml/badge.svg)](https://github.com/Fastering18/Manifast/actions/workflows/ci.yml)
https://github.com/Fastering18/Manifast  
Fast, scriptable programming language with Indonesian-based syntax, powered by LLVM.
**Current Version: 0.0.13**

## Syntax Samples
```manifast
-- Functions (Now with Type Annotations)
fungsi hitung(x: angka, y: angka): angka
    kembali x * y + 10
tutup

-- Static Types (i8, i16, i32, i64, f32, f64, char)
lokal hp: i32 = 100
lokal grade: char = 'A'

-- Compound Assignment (Works in VM & JIT)
hp -= 10
hp += 5

-- Custom Type Aliases & Structs
tipe bilangan = f64
tipe Orang = {
    nama: string,
    umur: bilangan
}
```

[Full Grammar Definition (BNF)](docs/GRAMMAR.md)

## Welcome Contributors!
We are open to anyone who wants to contribute to this project. Whether it's reporting bugs, suggesting features, or submitting pull requests, your help is appreciated!

## Performance Tiers

Manifast uses a multi-tier execution model to balance startup speed and raw performance:

| Tier | Engine | Target Use-case | Latency |
|------|--------|-----------------|---------|
| **Tier 0** | Bytecode VM | Scripting, Configs, Embedding, Web | **~50µs** startup |
| **Tier 1 (Core)** | AsmJit | Fast arithmetic, lightweight JIT | **~1-2ms** startup |
| **Tier 1 (Full)** | LLVM JIT | Heavy computation, scientific math | **~50-100ms** startup |

### Benchmarks
- **VM Startup**: ~0.05 ms (Full Lex/Parse/Compile/Run pipeline).
- **Core Memory**: < 500 KB (No LLVM dependencies).
- **Embedded API**: Fully thread-safe, multiple VM instances support.

## Quick Install
Get Manifast up and running instantly on your machine.

### Linux / macOS
```bash
curl -fsSL https://manifast.dev/install.sh | bash
```

### Windows (PowerShell)
```powershell
iwr -useb https://manifast.dev/install.ps1 | iex
```

---

## Feature Status
- [x] **Basic Syntax**: `fungsi`, `lokal`, `jika`, `selama`, `kembali`, etc.
- [x] **Type Annotations**: Optional static typing for better JIT optimization.
- [x] **Tables & Objects**: Dynamic objects with method injection.
- [x] **Error Handling**: `coba/tangkap` (Try/Catch) mechanism.
- [x] **Math Stdlib**: Extended MATLAB-style functions (`linspace`, `clamp`, `sin`, `log`, etc).
- [x] **Plot Module**: High-performance plotting (`plot.line`, `plot.scatter`, `plot.show`, `plotFor`).
- [x] **Custom Types**: User-defined type aliases (`tipe bilangan = f64`) and struct types.
- [x] **WebAssembly**: Fully integrated with auto-deploy to [Playground](https://fastering.thedev.id/Manifast/).
- [x] **Async Event API**: EventEmitter-style output handling for logs, errors, and plots in WASM.
- [x] **One-Liner Installers**: Quick distribution for all major platforms.
- [ ] IDE Support (LSP)
- [ ] Self compilation

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
Make sure `vcpkg` is installed and in your PATH. The `vcpkg.json` manages dependencies like `fmt` and `asmjit`.
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
By default, CMake builds `manifast_core` alongside the LLVM JIT. If you want a lightweight VM-only embedded build, configure with `-DMANIFAST_ENABLE_LLVM=OFF`.

**Standard Build (Clone & Go):**
```powershell
.\manifast.ps1 build
```

**Run scripts:**
```powershell
.\manifast.ps1 run script.mnf --stack-size 64mb
.\manifast.ps1 run-vm script.mnf
```

**Build WebAssembly:**
```powershell
.\manifast.ps1 build-wasm
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
