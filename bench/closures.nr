// closures and higher-order calls
make = unda() {
    n = 0
    rudisha unda() { n++ rudisha n }
}
c = make()
t = 0
kwa i ktk mfululizo(400000) { t += c() }
inc = unda(f, x) { rudisha f(x) + 1 }
id = unda(x) { rudisha x }
r = 0
kwa i ktk mfululizo(300000) { r = inc(id, r) }
andika(t, r)
