const fs = require('fs');
const path = require('path');

// Load the Emscripten-generated JS
const manifast = require('../../build-wasm/manifast.js');

async function runTest() {
  // Wait for the Wasm module to be ready
  await new Promise(resolve => {
    manifast.onRuntimeInitialized = resolve;
  });

  const code = `--assert(bukan salah, "bukan salah error "+ bukan salah)
println("Assertion Testing", println);
println("Assert passed.");

println("--- Type Testing (angka) ---")
println("Type of 123: " + tipe(123))

println("\n--- String Concatenation ---")
println("Halo " + "Dunia")
println("Umur: " + 25)

println("\n--- Standard Library Testing ---")
lokal os = impor("os")
println("Waktu OS: " + os.waktuNano())

lokal str = impor("string")
lokal parts = str.split("Manifast,Luar,Biasa", ",")

println("Split result 1: ", parts)
println("Split result 2: " + parts[2])
println("Split result 3: " + parts[3])

print("tipe parts ", tipe(parts))

println("\nSubstring 1-8: " + impor("string").substring("Manifast,Luar,Biasa", 1, 8))

println("\n--- OOP Testing ---")
kelas Orang maka
    fungsi inisiasi(nama, umur)
        self.nama = nama
        self.umur = umur
    tutup

    fungsi bicara()
        println("Halo, nama saya " + self.nama)
    tutup
tutup

lokal budi = Orang("Budi", 25)
budi.bicara()

println("Tipe budi: " + tipe(budi), budi, Orang)

-- Slicing test
lokal data = [10, 20, 30, 40, 50]
lokal sub = data[2:4]
println("Slice data[2:4]: " + tipe(sub), sub)
println("sub[1]: " + sub[1])
println("sub[2]: " + sub[2])
println("sub[3]: " + sub[3])

println("\n--- Recursion Testing ---")
fungsi fib(n)
  jika n < 2 maka
    kembali n
  tutup
  kembali fib(n-1) + fib(n-2)
tutup
local start = os.waktuNano()
print("fib(10) =", fib(21))
local end = os.waktuNano()
println("\nDone!", 5%2)
println("Time taken for fib recursive: " + (end - start)/1000 + "ms", start, end)
`;

  console.log("Running Manifast Test...");
  // Use ccall to call the C function mf_run_script
  // Output is now streamed via stdout (captured by Emscripten/Node)
  manifast.ccall('mf_run_script', 'string', ['string'], [code]);
  console.log("--------------");
}

runTest().catch(err => {
  console.error("Test Failed:", err);
});
