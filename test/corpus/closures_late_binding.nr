// closures see the defining scope, including variables created after the closure
make = unda() {
    reader = unda() { rudisha later }
    later = "defined after the closure"
    rudisha reader
}
andika(make()())

counter = unda() {
    n = 0
    rudisha [unda() { n++ rudisha n }, unda() { rudisha n }]
}
pair = counter()
pair[0]()
pair[0]()
andika(pair[1]())

other = counter()
andika(other[0](), pair[1]())

total = 0
add = unda(x) { total += x rudisha total }
add(5)
add(7)
andika(total)

shadow = unda() {
    total = 1000
    rudisha total
}
andika(shadow(), total)

fixed = unda() {
    fanya total = 50
    total += 1
    rudisha total
}
andika(fixed(), total)

outer = unda() {
    inner = unda() { rudisha x }
    x = 1
    first = inner()
    x = 2
    rudisha [first, inner()]
}
andika(outer())
