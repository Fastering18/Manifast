const events = require('events');
const manifast = require('../../build-wasm/manifast.js');

async function initializeVM() {
    await new Promise(resolve => {
        manifast.onRuntimeInitialized = resolve;
    });
    return manifast;
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

    const emitter = new events.EventEmitter();
    let currentOutput = "";
    let hasError = false;

    // Register event callback from C++
    // 0=log, 1=error, 2=plot, 3=clear, 4=done, 5=delay
    const eventCallback = vm.addFunction((type, dataPtr) => {
        const data = vm.UTF8ToString(dataPtr);
        if (type === 0) {
            currentOutput += data;
            emitter.emit('log', data);
        } else if (type === 1) {
            currentOutput += "\n[ERROR] " + data + "\n";
            hasError = true;
            emitter.emit('error', data);
        } else if (type === 4) {
            emitter.emit('done');
        }
    }, 'vpp');

    vm._mf_set_event_callback(eventCallback);

    // Use emitter to see logs in real-time if desired, or just aggregate
    emitter.on('log', (data) => {
        // We can print real-time if we want, but tests are usually cleaner with aggregated output
        // console.log("LOG:", data);
    });

    for (const suite of suites) {
        console.log(`[SUITE] ${suite.name}`);
        const suiteModule = require(suite.file);
        currentOutput = "";
        hasError = false;

        try {
            const result = vm.ccall('mf_run_script', 'string', ['string'], [suiteModule.code]);
            // If result contains error marker, we also mark as fail
            const ok = !hasError && !result.includes('[ERROR RUNTIME]') && !result.includes('Assertion Failed');

            console.log(ok ? '  [PASS]' : '  [FAIL]');
        } catch (e) {
            console.log('  [FAIL] Exception: ' + e.message);
            console.log('--- Output Captured ---');
            console.log(currentOutput);
            console.log('-----------------------');
        }
        console.log('------------------------------');
    }
}

runAll().catch(console.error);
