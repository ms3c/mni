// bubble sort: indexing and assignment
n = 1200
a = []
seed = 12345
kwa i ktk mfululizo(n) {
    seed = (seed * 1103515245 + 12345) % 2147483648
    a.sukuma(seed % 10000)
}
i = 0
wakati (i < n) {
    j = 0
    wakati (j < n - i - 1) {
        kama (a[j] > a[j + 1]) {
            t = a[j]
            a[j] = a[j + 1]
            a[j + 1] = t
        }
        j++
    }
    i++
}
andika(a[0], a[n - 1])
