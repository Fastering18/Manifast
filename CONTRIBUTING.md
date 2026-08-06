# Contributing to Manifast

Thank you for helping improve Manifast — a general-purpose and embeddable language with Indonesian-inspired syntax, a lightweight bytecode VM, and optional LLVM JIT/AOT.

## Development setup

See **[docs/INSTALLING.md](docs/INSTALLING.md)** for Windows (MSYS2 UCRT64) and Linux bootstraps.

```powershell
# Windows
.\manifast.ps1 bootstrap
.\manifast.ps1 build --fast
.\manifast.ps1 install-hooks   # enable pre-push quality gate
```

```bash
# Linux / macOS
./manifast.sh bootstrap
./manifast.sh build --fast
./scripts/install-hooks.sh
```

## Quality gate (required before push)

Every push must satisfy:

1. **All tests pass** — language suite + C++ unit tests  
2. **WASM playground rebuilt** — `docs/manifast.js`, `docs/manifast.wasm`, `docs/index.html` refreshed when core changes

Run the same gate locally:

```powershell
.\manifast.ps1 check
# or:
.\scripts\check-before-push.ps1
```

```bash
./scripts/check-before-push.sh
```

With hooks installed (`core.hooksPath=.githooks`), `git push` runs this automatically.

| Override | Meaning |
|----------|---------|
| `SKIP_PUSH_CHECKS=1` | Skip the pre-push hook (emergency only) |
| `-SkipWasm` / `--skip-wasm` | Skip WASM rebuild (not for normal releases) |

If the WASM rebuild changes `docs/`, **commit those assets** and push again. The gate will block a push that would leave the playground stale.

## Project layout

| Path | Role |
|------|------|
| `include/manifast/` | Public headers (runtime, VM, parser, JIT) |
| `src/lib/` | Core library sources |
| `src/cmd/` | `mifast` (CLI) and `mifastc` (AOT helper) |
| `src/wasm/` | Emscripten playground bindings + web UI |
| `docs/` | Published playground + grammar + install notes |
| `tests/` | Language (`.mnf`) and C++ unit tests |
| `cmake/` | Portable CMake modules |

## Embedding

Link `manifast_core` (VM-only, no LLVM) for embeddable use:

```bash
cmake -S . -B build -DMANIFAST_ENABLE_LLVM=OFF
cmake --build build --target manifast_core
```

Headers under `include/manifast/` expose the C API (`Runtime.h`) and VM types.

## Pull requests

1. Keep changes focused; prefer small, reviewable commits.  
2. Run `.\manifast.ps1 check` (or the shell equivalent) before opening a PR.  
3. Update docs when behavior, install steps, or public APIs change.  
4. CI runs Linux full build, Windows full + VM-only, and WASM.

## Communication

- Discord: https://discord.gg/8vdZsBBGRG  
- Issues / PRs: https://github.com/Fastering18/Manifast  
