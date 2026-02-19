#!/bin/bash

# Default values
BUILD_DIR="build"
COMMAND=$1
FAST_MODE=0

# Help message
show_help() {
    echo "Manifast Build Tool (Linux/macOS)"
    echo "Usage: ./manifast.sh <command> [options]"
    echo ""
    echo "Commands:"
    echo "  build       Configure and build the project"
    echo "  run         Run manifast code"
    echo "  test        Run the test suite"
    echo "  clean       Remove the build directory"
    echo "  help        Show this help message"
    echo ""
    echo "Options:"
    echo "  --fast     Disable vcpkg LLVM download (uses system LLVM)"
}

if [ -z "$COMMAND" ]; then
    show_help
    exit 1
fi

# Check for flags
for arg in "$@"; do
    if [ "$arg" == "--fast" ]; then
        FAST_MODE=1
    fi
done

if [ "$COMMAND" == "clean" ]; then
    echo "Cleaning build directory ($BUILD_DIR)..."
    rm -rf $BUILD_DIR
    echo "Done."
    exit 0
fi

if [ "$COMMAND" == "help" ]; then
    show_help
    exit 0
fi

if [ "$COMMAND" == "run" ]; then
    MNF_BIN="$BUILD_DIR/bin/manifast"
    if [ ! -f "$MNF_BIN" ]; then
        echo "Error: Binary not found. Run 'build' first."
        exit 1
    fi
    
    echo "Running Tests..."
    "$MNF_BIN" $2
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
    cmake --build $BUILD_DIR
    exit $?
fi

if [ "$COMMAND" == "test" ]; then
    TEST_BIN="$BUILD_DIR/bin/manifast"
    if [ ! -f "$TEST_BIN" ]; then
        echo "Error: Binary not found. Run 'build' first."
        exit 1
    fi
    
    echo "Running Tests..."
    "$TEST_BIN" tests/Manifast/test.mnf --test tests
    exit $?
fi

echo "Unknown command: $COMMAND"
show_help
exit 1
