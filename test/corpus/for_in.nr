// kwa ... ktk: sources, variable scope, snapshots and loop control
kwa v ktk [10, 20] { andika(v) }
kwa i, v ktk ["a", "b"] { andika(i, v) }
kwa c ktk "héy" { andika(c) }
kwa i, c ktk "hé" { andika(i, c) }
kwa v ktk {"x": 1, "y": 2} { andika(v) }
kwa k, v ktk {"x": 1, "y": 2} { andika(k, v) }

// loop variables live on in the surrounding scope
kwa item ktk [1, 2, 3] { }
andika(item)
kwa k, item ktk ["p", "q"] { }
andika(k, item)

// the container is copied: changes during the loop are not seen
a = [1, 2]
kwa v ktk a { a.sukuma(v * 10) }
andika(a)
d = {"a": 1}
kwa k, v ktk d { d["b"] = 2 }
andika(d)

// nesting, break and continue
kwa i ktk [1, 2, 3] {
    kwa j ktk [10, 20, 30] {
        kama (j == 20) { endelea }
        kama (i == 2) { vunja }
        andika(i, j)
    }
}

f = unda() {
    kwa v ktk [1, 2, 3] { kama (v == 2) { rudisha "returned " + "two" } }
    rudisha "never"
}
andika(f())
kwa v ktk mfululizo(3) { andika("r", v) }
kwa v ktk [] { andika("never") }
