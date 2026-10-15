// dictionaries and strings
keys = ["a", "b", "c", "d", "e", "f", "g", "h", "i", "j"]
counts = {}
kwa i ktk mfululizo(200000) {
    k = keys[i % 10] + keys[i % 7]
    kama (k ktk counts) { counts[k] += 1 } sivyo { counts[k] = 1 }
}
s = ""
kwa i ktk mfululizo(20000) { s += "ab" }
andika(counts["aa"], counts["gj"], s.idadi())
