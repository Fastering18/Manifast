#!/bin/bash

BUILD_DIR="build"
COMMAND=$1
FAST_MODE=0

show_help() {
    echo "Manifast Build Tool (Linux/macOS)"
    echo "Usage: ./manifast.sh <command> [options]"
    echo ""
    echo "Commands:"
    echo "  build       Configure and build the project"
    echo "  run         Run manifast file in jit tier"
    echo "  run-vm      Run manifast file in vm tier"
    echo "  test        Run the test suite"
    echo "  install     Install binaries to system and add to PATH"
    echo "  uninstall   Remove binaries and revert PATH changes"
    echo "  clean       Remove the build directory"
    echo "  build-wasm  Build for WebAssembly (requires Emscripten)"
    echo "  help        Show this help message"
    echo ""
    echo "Options:"
    echo "  --fast     Disable vcpkg LLVM download (uses system LLVM)"
}

if [ -z "$COMMAND" ]; then
    show_help
    exit 1
fi

shift
REMAINING_ARGS=("$@")

for arg in "${REMAINING_ARGS[@]}"; do
    if [ "$arg" == "--fast" ]; then
        FAST_MODE=1
    fi
done

if [ "$COMMAND" == "help" ]; then
    show_help
    exit 0
fi

if [ "$COMMAND" == "clean" ]; then
    echo "Cleaning build directory ($BUILD_DIR)..."
    rm -rf "$BUILD_DIR"
    echo "Done."
    exit 0
fi

if [ "$COMMAND" == "run" ]; then
    BIN="$BUILD_DIR/bin/mifast"
    if [ ! -f "$BIN" ]; then
        echo "Error: Binary not found. Run 'build' first."
        exit 1
    fi
    "$BIN" run "${REMAINING_ARGS[@]}"
    exit $?
fi

if [ "$COMMAND" == "run-vm" ]; then
    BIN="$BUILD_DIR/bin/mifast"
    if [ ! -f "$BIN" ]; then
        echo "Error: Binary not found. Run 'build' first."
        exit 1
    fi
    "$BIN" run "${REMAINING_ARGS[@]}" --vm
    exit $?
fi

if [ "$COMMAND" == "test" ]; then
    BIN="$BUILD_DIR/bin/mifast"
    if [ ! -f "$BIN" ]; then
        echo "Error: Binary not found. Run 'build' first."
        exit 1
    fi
    echo "Running Test Suite..."
    "$BIN" test "${REMAINING_ARGS[@]}"
    exit $?
fi

if [ "$COMMAND" == "build" ]; then
    if [ ! -d "$BUILD_DIR" ]; then
        echo "Configuring Project..."
        ARGS=("-S" "." "-B" "$BUILD_DIR" "-G" "Ninja")
        
        if [ $FAST_MODE -eq 1 ]; then
            echo "  Mode: FAST (System LLVM)"
            ARGS+=("-DVCPKG_MANIFEST_FEATURES=")
        else
            echo "  Mode: DEFAULT (Vcpkg Bundle)"
        fi
        
        cmake "${ARGS[@]}"
        if [ $? -ne 0 ]; then exit $?; fi
    fi
    
    echo "Building Project..."
    cmake --build "$BUILD_DIR" --parallel $(nproc 2>/dev/null || echo 4)
    exit $?
fi

if [ "$COMMAND" == "install" ]; then
    BIN_DIR="$BUILD_DIR/bin"
    LIB_DIR="$BUILD_DIR/lib"
    
    if [ ! -f "$BIN_DIR/mifast" ]; then
        echo "Error: Binary not found. Run 'build' first."
        exit 1
    fi
    
    INSTALL_DIR="$HOME/.local"
    # Support Termux
    if [ -n "$PREFIX" ]; then
        INSTALL_DIR="$PREFIX"
    fi
    INSTALL_BIN="$INSTALL_DIR/bin"
    INSTALL_LIB="$INSTALL_DIR/lib"
    
    echo "Installing Manifast to $INSTALL_DIR..."
    
    mkdir -p "$INSTALL_BIN" "$INSTALL_LIB"
    
    cp "$BIN_DIR/mifast" "$INSTALL_BIN/"
    [ -f "$BIN_DIR/mifastc" ] && cp "$BIN_DIR/mifastc" "$INSTALL_BIN/"
    cp "$LIB_DIR"/*.a "$INSTALL_LIB/" 2>/dev/null
    
    if ! echo "$PATH" | grep -q "$INSTALL_BIN"; then
        SHELL_RC="$HOME/.bashrc"
        [ -n "$ZSH_VERSION" ] && SHELL_RC="$HOME/.zshrc"
        echo "export PATH=\"$INSTALL_BIN:\$PATH\"" >> "$SHELL_RC"
        echo "Added $INSTALL_BIN to PATH in $SHELL_RC"
        echo "Run 'source $SHELL_RC' or restart your terminal."
    else
        echo "$INSTALL_BIN already in PATH."
    fi
    
    echo "Manifast installed successfully!"
    exit 0
fi

if [ "$COMMAND" == "uninstall" ]; then
    INSTALL_DIR="$HOME/.local"
    if [ -n "$PREFIX" ]; then
        INSTALL_DIR="$PREFIX"
    fi
    INSTALL_BIN="$INSTALL_DIR/bin"
    
    # Remove binaries
    rm -f "$INSTALL_BIN/mifast" "$INSTALL_BIN/mifastc"
    
    # Remove from shell rc
    SHELL_RC="$HOME/.bashrc"
    [ -n "$ZSH_VERSION" ] && SHELL_RC="$HOME/.zshrc"
    
    if [ -f "$SHELL_RC" ]; then
        # Use a temporary file to filter out the PATH line
        grep -v "export PATH=\"$INSTALL_BIN:\$PATH\"" "$SHELL_RC" > "$SHELL_RC.tmp" && mv "$SHELL_RC.tmp" "$SHELL_RC"
        echo "Removed Manifast PATH entry from $SHELL_RC"
    fi
    
    echo "Manifast uninstalled successfully."
    exit 0
fi

if [ "$COMMAND" == "build-wasm" ]; then
    WASM_BUILD_DIR="build-wasm"
    echo "Building WebAssembly..."
    
    if [ -d "$WASM_BUILD_DIR" ]; then
        rm -rf "$WASM_BUILD_DIR"
    fi
    
    if ! command -v emcmake &> /dev/null; then
        echo "Error: emcmake not found. Please activate Emscripten environment."
        exit 1
    fi
    
    echo "Configuring..."
    if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
       emcmake cmake -G "MinGW Makefiles" -S src/wasm -B "$WASM_BUILD_DIR"
    else
       emcmake cmake -S src/wasm -B "$WASM_BUILD_DIR"
    fi
    
    if [ $? -ne 0 ]; then exit $?; fi
    
    echo "Building..."
    cmake --build "$WASM_BUILD_DIR" --target manifast
    if [ $? -ne 0 ]; then exit $?; fi
    
    echo "WASM Build Success!"
    exit 0
fi

echo "Unknown command: $COMMAND"
show_help
exit 1
