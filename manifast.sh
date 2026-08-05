#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="build"
COMMAND="${1:-}"
FAST_MODE=0
PRESET=""
LLVM_DIR_ARG=""

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

show_help() {
    echo "Manifast Build Tool (Linux/macOS)"
    echo "Usage: ./manifast.sh <command> [options]"
    echo ""
    echo "Commands:"
    echo "  bootstrap   Install system packages (non-interactive apt/dnf)"
    echo "  build       Configure and build the project"
    echo "  run         Run manifast file in jit tier"
    echo "  run-vm      Run manifast file in vm tier"
    echo "  test        Run the test suite"
    echo "  install     Install binaries to ~/.local (or \$PREFIX)"
    echo "  uninstall   Remove binaries and PATH entry"
    echo "  clean       Remove the build directory"
    echo "  build-wasm  Build for WebAssembly (requires Emscripten)"
    echo "  help        Show this help message"
    echo ""
    echo "Options:"
    echo "  --fast              Use system LLVM (skip vcpkg LLVM bundle)"
    echo "  --preset <name>     CMake preset (linux-system, no-llvm, vcpkg, ...)"
    echo "  --llvm-dir <path>   Explicit LLVMConfig.cmake directory"
}

if [ -z "$COMMAND" ]; then
    show_help
    exit 1
fi

shift
REMAINING_ARGS=()
while [ $# -gt 0 ]; do
    case "$1" in
        --fast)
            FAST_MODE=1
            shift
            ;;
        --preset)
            PRESET="${2:-}"
            shift 2
            ;;
        --llvm-dir)
            LLVM_DIR_ARG="${2:-}"
            shift 2
            ;;
        *)
            REMAINING_ARGS+=("$1")
            shift
            ;;
    esac
done

ensure_cmake() {
    if ! command -v cmake >/dev/null 2>&1; then
        echo "Error: cmake not found. Run: ./manifast.sh bootstrap"
        exit 1
    fi
}

find_vcpkg_toolchain() {
    if [ -n "${VCPKG_ROOT:-}" ] && [ -f "$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" ]; then
        echo "$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
        return
    fi
    if command -v vcpkg >/dev/null 2>&1; then
        local root
        root="$(dirname "$(dirname "$(command -v vcpkg)")")"
        if [ -f "$root/scripts/buildsystems/vcpkg.cmake" ]; then
            echo "$root/scripts/buildsystems/vcpkg.cmake"
            return
        fi
    fi
}

case "$COMMAND" in
help)
    show_help
    exit 0
    ;;
bootstrap)
    bash "$SCRIPT_DIR/scripts/bootstrap-linux.sh"
    exit $?
    ;;
clean)
    echo "Cleaning build directory ($BUILD_DIR)..."
    rm -rf "$BUILD_DIR"
    echo "Done."
    exit 0
    ;;
run)
    BIN="$BUILD_DIR/bin/mifast"
    if [ ! -f "$BIN" ]; then
        echo "Error: Binary not found. Run 'build' first."
        exit 1
    fi
    "$BIN" run "${REMAINING_ARGS[@]+"${REMAINING_ARGS[@]}"}"
    exit $?
    ;;
run-vm)
    BIN="$BUILD_DIR/bin/mifast"
    if [ ! -f "$BIN" ]; then
        echo "Error: Binary not found. Run 'build' first."
        exit 1
    fi
    "$BIN" run "${REMAINING_ARGS[@]+"${REMAINING_ARGS[@]}"}" --vm
    exit $?
    ;;
test)
    BIN="$BUILD_DIR/bin/mifast"
    if [ ! -f "$BIN" ]; then
        echo "Error: Binary not found. Run 'build' first."
        exit 1
    fi
    echo "Running Test Suite..."
    "$BIN" test "${REMAINING_ARGS[@]+"${REMAINING_ARGS[@]}"}"
    status=$?
    if command -v ctest >/dev/null 2>&1; then
        echo "Running CTest..."
        ctest --test-dir "$BUILD_DIR" --output-on-failure || status=$?
    fi
    exit $status
    ;;
build)
    ensure_cmake
    MODE_FILE="$BUILD_DIR/build_mode.txt"
    if [ -n "$PRESET" ]; then
        CURRENT_MODE="PRESET:$PRESET"
    elif [ "$FAST_MODE" -eq 1 ]; then
        CURRENT_MODE="FAST"
    else
        CURRENT_MODE="DEFAULT"
    fi

    NEEDS_CONFIG=1
    if [ -f "$BUILD_DIR/build.ninja" ] || [ -f "$BUILD_DIR/CMakeCache.txt" ]; then
        if [ -f "$MODE_FILE" ] && [ "$(cat "$MODE_FILE")" = "$CURRENT_MODE" ]; then
            NEEDS_CONFIG=0
        fi
    fi

    if [ "$NEEDS_CONFIG" -eq 1 ]; then
        echo "Configuring Project..."
        mkdir -p "$BUILD_DIR"
        printf '%s' "$CURRENT_MODE" > "$MODE_FILE"

        if [ -n "$PRESET" ]; then
            echo "  Mode: preset $PRESET"
            cmake --preset "$PRESET"
        elif [ "$FAST_MODE" -eq 1 ]; then
            echo "  Mode: FAST (System LLVM)"
            ARGS=("-S" "." "-B" "$BUILD_DIR" "-G" "Ninja" "-DVCPKG_MANIFEST_FEATURES=")
            if [ -n "$LLVM_DIR_ARG" ]; then
                ARGS+=("-DLLVM_DIR=$LLVM_DIR_ARG")
            elif [ -d /usr/lib/llvm-18/lib/cmake/llvm ]; then
                ARGS+=("-DLLVM_DIR=/usr/lib/llvm-18/lib/cmake/llvm")
            fi
            cmake "${ARGS[@]}"
        else
            echo "  Mode: DEFAULT (vcpkg if available)"
            ARGS=("-S" "." "-B" "$BUILD_DIR" "-G" "Ninja")
            TOOLCHAIN="$(find_vcpkg_toolchain || true)"
            if [ -n "${TOOLCHAIN:-}" ]; then
                echo "  Using vcpkg: $TOOLCHAIN"
                ARGS+=("-DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN")
            fi
            if [ -n "$LLVM_DIR_ARG" ]; then
                ARGS+=("-DLLVM_DIR=$LLVM_DIR_ARG")
            fi
            cmake "${ARGS[@]}"
        fi
    fi

    echo "Building Project..."
    JOBS="$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
    cmake --build "$BUILD_DIR" --parallel "$JOBS"
    exit $?
    ;;
install)
    BIN_DIR="$BUILD_DIR/bin"
    LIB_DIR="$BUILD_DIR/lib"
    if [ ! -f "$BIN_DIR/mifast" ]; then
        echo "Error: Binary not found. Run 'build' first."
        exit 1
    fi
    INSTALL_DIR="${PREFIX:-$HOME/.local}"
    INSTALL_BIN="$INSTALL_DIR/bin"
    INSTALL_LIB="$INSTALL_DIR/lib"
    echo "Installing Manifast to $INSTALL_DIR..."
    mkdir -p "$INSTALL_BIN" "$INSTALL_LIB"
    cp "$BIN_DIR/mifast" "$INSTALL_BIN/"
    [ -f "$BIN_DIR/mifastc" ] && cp "$BIN_DIR/mifastc" "$INSTALL_BIN/"
    cp "$LIB_DIR"/*.a "$INSTALL_LIB/" 2>/dev/null || true
    if ! echo "${PATH:-}" | grep -q "$INSTALL_BIN"; then
        SHELL_RC="$HOME/.bashrc"
        [ -n "${ZSH_VERSION:-}" ] && SHELL_RC="$HOME/.zshrc"
        echo "export PATH=\"$INSTALL_BIN:\$PATH\"" >> "$SHELL_RC"
        echo "Added $INSTALL_BIN to PATH in $SHELL_RC"
    fi
    echo "Manifast installed successfully!"
    exit 0
    ;;
uninstall)
    INSTALL_DIR="${PREFIX:-$HOME/.local}"
    INSTALL_BIN="$INSTALL_DIR/bin"
    rm -f "$INSTALL_BIN/mifast" "$INSTALL_BIN/mifastc"
    SHELL_RC="$HOME/.bashrc"
    [ -n "${ZSH_VERSION:-}" ] && SHELL_RC="$HOME/.zshrc"
    if [ -f "$SHELL_RC" ]; then
        grep -v "export PATH=\"$INSTALL_BIN:\$PATH\"" "$SHELL_RC" > "$SHELL_RC.tmp" && mv "$SHELL_RC.tmp" "$SHELL_RC"
        echo "Removed Manifast PATH entry from $SHELL_RC"
    fi
    echo "Manifast uninstalled successfully."
    exit 0
    ;;
build-wasm)
    WASM_BUILD_DIR="build-wasm"
    echo "Building WebAssembly..."
    rm -rf "$WASM_BUILD_DIR"
    if ! command -v emcmake >/dev/null 2>&1; then
        if [ -n "${EMSDK:-}" ] && [ -f "$EMSDK/emsdk_env.sh" ]; then
            # shellcheck disable=SC1091
            source "$EMSDK/emsdk_env.sh"
        else
            echo "Error: emcmake not found. Activate Emscripten or set EMSDK."
            exit 1
        fi
    fi
    emcmake cmake -G Ninja -S src/wasm -B "$WASM_BUILD_DIR"
    cmake --build "$WASM_BUILD_DIR" --target manifast
    echo "WASM Build Success!"
    exit 0
    ;;
*)
    echo "Unknown command: $COMMAND"
    show_help
    exit 1
    ;;
esac
