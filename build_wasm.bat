@echo off
echo Cleaning build-wasm...
if exist build-wasm rmdir /s /q build-wasm
if exist build-wasm (
    echo Failed to delete build-wasm. Please close any programs using it.
    exit /b 1
)

echo Activating Emscripten...
call D:\Program\Emscripten\emsdk\emsdk_env.bat

echo Configuring with emcmake...
call emcmake cmake -G "MinGW Makefiles" -S src/wasm -B build-wasm
if %errorlevel% neq 0 exit /b %errorlevel%

echo Building Manifast WASM...
cmake --build build-wasm --target manifast
if %errorlevel% neq 0 exit /b %errorlevel%

echo Running Tests...
node tests/wasm/test_wasm.js
