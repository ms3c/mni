// evaluation order of compound assignment and the value of ++/--
log = []
note = unda(tag, v) { log.sukuma(tag) rudisha v }

a = [10, 20, 30]
a[note("index", 1)] += note("value", 5)
andika(a, log)

log = []
d = {"k": 1}
d[note("key", "k")] *= note("factor", 7)
andika(d, log)

i = 5
andika(i++, i, i--, i)
j = 1.5
andika(j++, j)
b = [1, 2]
andika(b[0]++, b)
pakeji Q { n = 10 }
andika(Q.n++, Q.n)
Q.n += 5
andika(Q.n)

x = 1
x += x + x
andika(x)
s = "ab"
s += "cd"
s *= 2
andika(s)
l = [1]
l += [2, 3]
andika(l)
m = {"a": 1}
m += {"b": 2, "a": 9}
andika(m)
v = 10
v /= 4
andika(v)
v = 10
v %= 4
andika(v)
