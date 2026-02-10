exports.code = `
lokal a = benar
lokal b = salah

assert((a dan b) == salah, "benar dan salah harusnya salah")
assert((a atau b) == benar, "benar atau salah harusnya benar")
assert((b atau benar) == benar, "salah atau benar harusnya benar")
assert((b dan benar) == salah, "salah dan benar harusnya salah")

-- Short circuit check
lokal sideEffectCalled = salah
fungsi sideEffect()
  sideEffectCalled = benar
  kembali benar
tutup

lokal x = benar atau sideEffect()
assert(sideEffectCalled == salah, "atau harusnya short circuit (benar atau ...)")

lokal y = salah dan sideEffect()
assert(sideEffectCalled == salah, "dan harusnya short circuit (salah dan ...)")
`;
