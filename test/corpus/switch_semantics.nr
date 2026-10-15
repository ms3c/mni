// badili: ordering, default placement, equality rules, no fallthrough
show = unda(v) {
    badili (v) {
        kawaida { rudisha "default" }
        ikiwa 1, 2 { rudisha "one-or-two" }
        ikiwa "a" { rudisha "letter a" }
        ikiwa kweli { rudisha "true" }
    }
}
andika(show(1), show(2), show(1.0), show("a"), show(kweli), show(7), show(tupu))

calls = []
side = unda(x) { calls.sukuma(x) rudisha x }
badili (3) {
    ikiwa side(1), side(2), side(3), side(4) { andika("matched") }
    ikiwa side(5) { andika("not reached") }
}
andika(calls)

kwa i ktk [1, 2, 3, 4] {
    badili (i) {
        ikiwa 2 { endelea }
        ikiwa 3 { vunja }
        kawaida { andika("i is", i) }
    }
    andika("end of iteration", i)
}

x = badili ([1, 2]) { ikiwa [1, 2] { "arrays compare by value" } kawaida { "no" } }
andika(x)
