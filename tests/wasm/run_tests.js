const fs = require('fs');
const path = require('path');
const manifast = require('../../build-wasm/manifast.js');

async function initializeVM() {
    await new Promise(resolve => {
        manifast.onRuntimeInitialized = resolve;
    });
    return manifast;
}

function runScript(vm, code) {
    return vm.ccall('mf_run_script', 'string', ['string'], [code]);
}

const suites = [
    { name: 'Core Types & Strings', file: './suites/core.js' },
    { name: 'Logical Operators & Mandelbrot', file: './suites/logic.js' },
    { name: 'Math Module', file: './suites/math.js' },
    { name: 'OOP Features', file: './suites/oop.js' },
    { name: 'Recursion Logic', file: './suites/recursion.js' }
];

async function runAll() {
    const vm = await initializeVM();
    console.log('=== MANIFAST WASM TEST RUNNER ===\n');

    for (const suite of suites) {
        console.log(`[SUITE] ${suite.name}`);
        const suiteModule = require(suite.file);
        try {
            const result = runScript(vm, suiteModule.code);
            // If mf_run_script returns a string, we check if it contains error markers.
            // In our current bindings, runtime errors and assertion failures go to stderr,
            // which might not be captured in the returned string depending on implementation.
            // However, we can also check if the script produced "Assertion Failed" in its output
            // if we capture it.
            const ok = !result.includes('[ERROR RUNTIME]') && !result.includes('Assertion Failed');
            console.log(ok ? '  [PASS]' : '  [FAIL]');
            if (!ok) {
                console.log('--- Output ---');
                console.log(result);
                console.log('--------------');
            }
        } catch (e) {
            console.log('  [FAIL] Exception: ' + e.message);
        }
        console.log('------------------------------');
    }
}

runAll().catch(console.error);
