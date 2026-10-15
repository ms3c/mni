// the value of blocks, ifs, loops and function bodies
a = kama (kweli) { 1 2 3 }
b = kama (sikweli) { 1 }
c = kama (sikweli) { 1 } sivyo { 2 4 }
andika(a, b, c)

f1 = unda() { 1 2 }
f2 = unda() { fanya t = 5 }
f3 = unda() { wakati (sikweli) { } }
f4 = unda() { rudisha }
f5 = unda() { kwa v ktk [1, 2] { v } }
f6 = unda() { }
f7 = unda() { kama (kweli) { "yes" } }
f8 = unda() { rudisha 1 2 }
andika(f1(), f2(), f3(), f4(), f5(), f6(), f7(), f8())

d = badili (2) { ikiwa 1 { "one" } ikiwa 2 { "two" } }
e = badili (9) { ikiwa 1 { "one" } }
andika(d, e)
andika(aina(f2()), aina(f6()))
r = 0
q = wakati (r < 3) { r++ }
andika(q, r)
