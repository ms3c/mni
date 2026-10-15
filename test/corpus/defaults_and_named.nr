// default parameters and named arguments
counter = 0
next = unda() { counter++ rudisha counter }

f = unda(a, b = a * 2, c = next()) { rudisha [a, b, c] }
andika(f(1), f(1, 5), f(1, 5, 9), counter)

g = unda(x = next(), y = x + 100) { rudisha [x, y] }
andika(g(), g(7), counter)

pakeji P {
    h = unda(a, b = 10, c = 100) { rudisha a + b + c }
    k = unda(first, second = "s") { rudisha first + second }
}
andika(P.h(1), P.h(1, c = 5), P.h(c = 1, a = 2), P.h(1, b = 2, c = 3))
andika(P.k("x"), P.k(second = "-", first = "y"))
andika([P.h(1), counter])

order = []
note = unda(v) { order.sukuma(v) rudisha v }
P.h(note(1), c = note(3), b = note(2))
andika(order)
