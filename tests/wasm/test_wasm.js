const fs = require('fs');
const path = require('path');

// Load the Emscripten-generated JS
const manifast = require('../../docs/manifast.js');

async function runTest() {
    // Wait for the Wasm module to be ready
    await new Promise(resolve => {
        manifast.onRuntimeInitialized = resolve;
    });

    const code = `
fungsi recursive(n)
  jika n < 2 maka
    kembali n
  tutup
  println(n)
  kembali recursive(n-1)
tutup

print("Start...")
println(recursive(4))
print("Done!")
`;

    console.log("Running Manifast Test...");
    // Use ccall to call the C function mf_run_script
    // const char* mf_run_script(const char* source)
    const result = manifast.ccall('mf_run_script', 'string', ['string'], [code]);

    console.log("--- Output ---");
    console.log(result);
    console.log("--------------");
}

runTest().catch(err => {
    console.error("Test Failed:", err);
});
