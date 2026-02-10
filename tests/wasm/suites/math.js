exports.code = `
lokal math = impor("math")

assert(math.pi > 3.14 dan math.pi < 3.15, "math.pi salah")
assert(math.e > 2.71 dan math.e < 2.72, "math.e salah")

-- Floating point tolerance helper
fungsi near(a, b)
  lokal diff = a - b
  jika (diff < 0) diff = 0 - diff tutup
  kembali diff < 0.0001
tutup

assert(near(math.sin(math.pi / 2), 1), "sin(pi/2) harusnya 1")
assert(near(math.cos(math.pi), -1), "cos(pi) harusnya -1")
assert(math.abs(-5.5) == 5.5, "abs(-5.5) harusnya 5.5")
assert(math.sqrt(16) == 4, "sqrt(16) harusnya 4")
assert(math.pow(2, 3) == 8, "pow(2, 3) harusnya 8")
assert(math.floor(4.9) == 4, "floor(4.9) harusnya 4")
assert(math.ceil(4.1) == 5, "ceil(4.1) harusnya 5")
`;
