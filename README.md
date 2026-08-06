# Manifast

[![Build and Test](https://github.com/Fastering18/Manifast/actions/workflows/ci.yml/badge.svg)](https://github.com/Fastering18/Manifast/actions/workflows/ci.yml)

**Manifast** is a fast, scriptable programming language with Indonesian-inspired syntax. It ships as both a **general-purpose CLI language** and an **embeddable runtime** (lightweight bytecode VM, optional LLVM JIT/AOT, and WebAssembly playground).

| | |
|--|--|
| **Version** | 0.0.13 |
| **Repository** | https://github.com/Fastering18/Manifast |
| **Playground** | https://fastering.thedev.id/Manifast/ |
| **Grammar** | [docs/GRAMMAR.md](docs/GRAMMAR.md) |

---

## Why Manifast?

- **Readable syntax** — keywords such as `fungsi`, `jika`, `selama`, `kembali`, with optional static types.
- **Multi-tier execution** — start in a tiny VM; promote hot code to AsmJit or full LLVM JIT when you need throughput.
- **Embeddable core** — `manifast_core` builds without LLVM for host apps, configs, plugins, and WASM.
- **Cross-platform** — native Windows and Linux (no WSL required); browser via WebAssembly.
- **Batteries** — math helpers, plotting, objects/classes, try/catch, and a web playground.

---

## Quick install (binaries)

### Linux / macOS

```bash
curl -fsSL https://raw.githubusercontent.com/Fastering18/Manifast/master/install.sh | bash
```

### Windows (PowerShell)

```powershell
iwr -useb https://raw.githubusercontent.com/Fastering18/Manifast/master/install.ps1 | iex
```

Then:

```text
mifast run script.mnf
mifast run script.mnf --vm
```

---

## Language sample

```manifast
fungsi hitung(x: angka, y: angka): angka
    kembali x * y + 10
tutup

lokal hp: i32 = 100
hp -= 10

tipe bilangan = f64
tipe Orang = {
    nama: string,
    umur: bilangan
}
```

---

## Execution tiers

| Tier | Engine | Typical use | Startup |
|------|--------|-------------|---------|
| **0** | Bytecode VM | Scripting, config, embedding, WASM | ~50 µs |
| **1 Core** | AsmJit | Fast arithmetic, light JIT | ~1–2 ms |
| **1 Full** | LLVM JIT | Heavy compute, scientific work | ~50–100 ms |

Approximate: VM pipeline &lt; 0.1 ms cold start; core footprint without LLVM under ~500 KB.

---

## Build from source

Manifast is a standard **C++20 / CMake 3.25+** project.

### Requirements

- CMake, Ninja, C++20 compiler  
- **LLVM 18+** for JIT/AOT (optional — use `-DMANIFAST_ENABLE_LLVM=OFF` for VM-only)  
- **fmt** (and optionally asmjit, gtest)  
- **Emscripten** only if you rebuild the playground WASM  

Full toolchains: **[docs/INSTALLING.md](docs/INSTALLING.md)**.

### Windows (recommended)

```powershell
git clone https://github.com/Fastering18/Manifast
cd Manifast
.\manifast.ps1 bootstrap          # winget + MSYS2 UCRT64 (non-GUI)
.\manifast.ps1 build --fast       # system/MSYS2 LLVM
.\manifast.ps1 install-hooks      # pre-push: tests + WASM
```

### Linux

```bash
git clone https://github.com/Fastering18/Manifast
cd Manifast
./manifast.sh bootstrap
./manifast.sh build --fast
./scripts/install-hooks.sh
```

### Helper commands

| Task | Windows | Linux / macOS |
|------|---------|----------------|
| Bootstrap tools | `.\manifast.ps1 bootstrap` | `./manifast.sh bootstrap` |
| Build (system LLVM) | `.\manifast.ps1 build --fast` | `./manifast.sh build --fast` |
| Run (JIT) | `.\manifast.ps1 run s.mnf` | `./manifast.sh run s.mnf` |
| Run (VM) | `.\manifast.ps1 run-vm s.mnf` | `./manifast.sh run-vm s.mnf` |
| Tests | `.\manifast.ps1 test` | `./manifast.sh test` |
| **Pre-push gate** | `.\manifast.ps1 check` | `./scripts/check-before-push.sh` |
| WASM playground | `.\manifast.ps1 build-wasm` | `./manifast.sh build-wasm` |

VM-only embeddable library:

```bash
cmake --preset no-llvm
cmake --build build --target manifast_core
```

---

## Quality gate (push policy)

Before code reaches `master`, the following must hold:

1. **`mifast test`** and **`ctest`** both pass.  
2. **WASM playground** is rebuilt into `docs/` (`manifast.js`, `manifast.wasm`, `index.html`).

```powershell
.\manifast.ps1 check
```

Git hook (after `install-hooks`):

- `pre-push` runs the same checks automatically.  
- Emergency bypass: `SKIP_PUSH_CHECKS=1 git push` (not for normal work).

CI mirrors this: Linux + Windows native builds/tests, Windows VM-only, and a dedicated **WASM** job.

See **[CONTRIBUTING.md](CONTRIBUTING.md)** for contributor workflow details.

---

## Feature status

| Area | Status |
|------|--------|
| Core syntax (`fungsi`, `lokal`, `jika`, `selama`, …) | Done |
| Optional type annotations | Done |
| Objects / classes | Done |
| `coba` / `tangkap` | Done |
| Math stdlib | Done |
| Plot module | Done |
| Custom types / structs | Done |
| WebAssembly playground | Done |
| One-line installers | Done |
| LSP / IDE | Planned |
| Self-hosting | Planned |

---

## Embedding

Use the C/C++ runtime API from `include/manifast/Runtime.h` and link `manifast_core` for a host-friendly, LLVM-free VM. Multiple VM instances are supported for concurrent embedding scenarios.

---

## Documentation

| Document | Contents |
|----------|----------|
| [docs/INSTALLING.md](docs/INSTALLING.md) | Toolchains, vcpkg, presets, Windows/Linux |
| [docs/GRAMMAR.md](docs/GRAMMAR.md) | Language grammar (BNF) |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Hooks, PR checklist, layout |
| [Playground](https://fastering.thedev.id/Manifast/) | In-browser IDE |

---

## Community

[Discord](https://discord.gg/8vdZsBBGRG) · [GitHub Issues](https://github.com/Fastering18/Manifast/issues)
