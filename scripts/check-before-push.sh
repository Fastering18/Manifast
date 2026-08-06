#!/usr/bin/env bash
# Quality gate before push: full tests must pass, then rebuild WASM into docs/.
# Usage: ./scripts/check-before-push.sh [--skip-wasm] [--skip-tests]
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

SKIP_WASM=0
SKIP_TESTS=0
for arg in "$@"; do
  case "$arg" in
    --skip-wasm) SKIP_WASM=1 ;;
    --skip-tests) SKIP_TESTS=1 ;;
  esac
done

step() { echo "==> $*"; }
fail() { echo "FAIL: $*" >&2; exit 1; }

find_mifast() {
  if [[ -x build/bin/mifast ]]; then echo "build/bin/mifast"; return; fi
  if [[ -x build/bin/mifast.exe ]]; then echo "build/bin/mifast.exe"; return; fi
  return 1
}

if ! MIFAST="$(find_mifast)"; then
  step "No mifast binary — building..."
  ./manifast.sh build --fast || fail "native build failed"
  MIFAST="$(find_mifast)" || fail "mifast still missing after build"
fi

if [[ "$SKIP_TESTS" -eq 0 ]]; then
  step "Running language test suite (mifast test)..."
  "$MIFAST" test || fail "mifast test failed"

  step "Running CTest unit tests..."
  command -v ctest >/dev/null || fail "ctest not on PATH"
  [[ -d build ]] || fail "build/ missing for ctest"
  ctest --test-dir build --output-on-failure || fail "ctest failed"
  echo "  Tests OK"
else
  echo "  WARN: tests skipped (--skip-tests)"
fi

if [[ "$SKIP_WASM" -eq 1 ]]; then
  echo "  WARN: WASM rebuild skipped (--skip-wasm)"
else
  step "Rebuilding WASM playground assets..."
  if ! command -v emcmake >/dev/null 2>&1; then
    if [[ -n "${EMSDK:-}" && -f "$EMSDK/emsdk_env.sh" ]]; then
      # shellcheck disable=SC1091
      source "$EMSDK/emsdk_env.sh"
    elif [[ -f "$HOME/emsdk/emsdk_env.sh" ]]; then
      # shellcheck disable=SC1091
      source "$HOME/emsdk/emsdk_env.sh"
    else
      fail "Emscripten not found. Install EMSDK or set EMSDK. Use --skip-wasm only in emergencies."
    fi
  fi
  ./manifast.sh build-wasm || fail "build-wasm failed"
  for asset in docs/manifast.js docs/manifast.wasm docs/index.html; do
    [[ -f "$asset" ]] || fail "missing required web asset after WASM build: $asset"
  done
  if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    dirty="$(git status --porcelain -- docs/ src/wasm/ || true)"
    if [[ -n "$dirty" ]]; then
      echo "$dirty"
      fail "WASM/web assets changed. Commit docs/ (and src/wasm if needed), then push again."
    fi
  fi
  echo "  WASM/docs OK"
fi

echo ""
echo "Pre-push checks passed."
