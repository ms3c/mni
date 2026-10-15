// break, continue and return inside expression operands
i = 0
wakati (i < 6) {
    i++
    x = 10 + kama (i == 2) { endelea } sivyo { 1 }
    andika("i =", i, "x =", x)
    y = 1 + kama (i == 4) { vunja } sivyo { 0 }
    andika("y =", y)
}
andika("after loop", i)

f = unda() {
    z = 5 + kama (kweli) { rudisha "early" } sivyo { 1 }
    rudisha "late"
}
andika(f())

g = unda() {
    kwa v ktk [1, 2, 3] {
        t = [v, kama (v == 2) { vunja } sivyo { v * 10 }]
        andika("t", t)
    }
    rudisha "g done"
}
andika(g())

h = unda() {
    n = 0
    wakati (kweli) {
        n++
        andika("n", n, 100 + kama (n >= 3) { vunja } sivyo { 0 })
    }
    rudisha n
}
andika(h())
