x = 1
reads_then_shadows = unda() {
    a = x
    fanya x = 9
    rudisha [a, x]
}
andika(reads_then_shadows(), x)

assigns_global = unda() { x = x + 1; rudisha x }
andika(assigns_global(), assigns_global(), x)

makes_local = unda() { fresh = 7; rudisha fresh }
andika(makes_local())
andika(muda_ni_huu = 0)

compound = unda(n) { n += 2; x += n; rudisha [n, x] }
andika(compound(3), x)

loops = unda(items) {
    total = 0
    kwa k, v ktk items { total += v; last = k }
    kwa c ktk "ab" { total = total + 1 }
    rudisha [total, last, c]
}
andika(loops([5, 6, 7]), loops({"p": 1, "q": 2}))

same = unda(a, a) { rudisha a }
andika(same(1, 2))

defaults = unda(a, b = a + 1, c = b * 2) { rudisha [a, b, c] }
andika(defaults(1), defaults(1, 5), defaults(1, c = 0))

fib = unda(n) {
    kama (n <= 1) { rudisha n }
    rudisha fib(n - 1) + fib(n - 2)
}
andika(fib(15))

pakeji P {
    f = unda() { fresh = 7; rudisha fresh }
    g = unda() { seen = n; rudisha seen }
    n = 3
}
andika(P.f(), P.g())

undefined_inside = unda() { rudisha nowhere }
andika(undefined_inside())
