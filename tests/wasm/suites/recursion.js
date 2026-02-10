exports.code = `
fungsi fib(n)
  jika n < 2 maka
    kembali n
  tutup
  kembali fib(n-1) + fib(n-2)
tutup

assert(fib(7) == 13, "fib(7) harusnya 13")
assert(fib(10) == 55, "fib(10) harusnya 55")
`;
