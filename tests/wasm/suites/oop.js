exports.code = `
kelas Robot maka
  fungsi inisiasi(nama)
    self.nama = nama
    self.daya = 100
  tutup

  fungsi cas()
    self.daya = self.daya + 10
  tutup
tutup

lokal blast = Robot("Blast")
assert(blast.nama == "Blast", "nama robot salah")
assert(blast.daya == 100, "daya awal salah")
blast.cas()
assert(blast.daya == 110, "daya setelah cas salah")
`;
