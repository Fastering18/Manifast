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
curl -fsSL https://raw.githubusercontent.com/Fastering18/Manifast/master/install.sh | bash
```

### Windows (PowerShell)
```powershell
iwr -useb https://raw.githubusercontent.com/Fastering18/Manifast/master/install.ps1 | iex
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

Manifast is a **standard cross-platform C++20 CMake project**. It builds natively on **Windows** and **Linux** (no WSL required).

### Requirements
- CMake 3.25+
- Ninja (or another CMake generator)
- C++20 compiler (MSVC, MinGW/UCRT, or GCC/Clang)
- **LLVM 18+** for JIT/AOT (optional — use VM-only builds without it)
- **fmt** (and optionally asmjit, gtest)

### Clone
```powershell
git clone https://github.com/Fastering18/Manifast
cd Manifast
```

### Windows (recommended)

Non-GUI bootstrap (winget + MSYS2 UCRT64 prebuilt LLVM):

```powershell
.\manifast.ps1 bootstrap
# ensure UCRT64 bin is on PATH, then:
.\manifast.ps1 build --fast
```

### Linux

```bash
./manifast.sh bootstrap
./manifast.sh build --fast
```

Details and alternatives (vcpkg, VM-only, presets): **[docs/INSTALLING.md](docs/INSTALLING.md)**.

### Build & run helpers

| Command | Windows | Linux/macOS |
|---------|---------|-------------|
| Bootstrap tools | `.\manifast.ps1 bootstrap` | `./manifast.sh bootstrap` |
| Build (system LLVM) | `.\manifast.ps1 build --fast` | `./manifast.sh build --fast` |
| Run JIT | `.\manifast.ps1 run script.mnf` | `./manifast.sh run script.mnf` |
| Run VM | `.\manifast.ps1 run-vm script.mnf` | `./manifast.sh run-vm script.mnf` |
| Tests | `.\manifast.ps1 test` | `./manifast.sh test` |
| WASM | `.\manifast.ps1 build-wasm` | `./manifast.sh build-wasm` |
| Clean | `.\manifast.ps1 clean` | `./manifast.sh clean` |

VM-only (no LLVM):

```bash
cmake --preset no-llvm
cmake --build build
```

---

Join our conversation here for development: [Discord Server](https://discord.gg/8vdZsBBGRG)
