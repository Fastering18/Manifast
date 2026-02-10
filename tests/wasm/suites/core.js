exports.code = `
assert(tipe(123) == "angka", "tipe(123) harusnya angka")
assert(tipe("Halo") == "string", "tipe('Halo') harusnya string")
assert(tipe(benar) == "bool", "tipe(benar) harusnya bool")
assert(tipe(nil) == "nil", "tipe(nil) harusnya nil")

lokal str = impor("string")
assert(str.substring("ManifastLuarBiasa", 1, 8) == "Manifast", "substring gagal")
lokal p = str.split("Luar,Biasa", ",")
assert(p[1] == "Luar", "split gagal indeks 1")
assert(p[2] == "Biasa", "split gagal indeks 2")
`;
