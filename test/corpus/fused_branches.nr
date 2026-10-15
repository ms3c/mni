c = kweli
d = sikweli
andika(kama (kama (c) { 1 < 2 } sivyo { 3 < 2 }) { "yes" } sivyo { "no" })
andika(kama (kama (d) { 1 < 2 } sivyo { 3 < 2 }) { "yes" } sivyo { "no" })
i = 0
wakati (i < 3) { i += 1 }
andika(i, 5 >= i, i != 3, 2.5 < i, "a" < "b", i == 3.0)
andika(kama (i <= 2) { 1 } sivyo { 2 })
andika(kama ("a" == "a") { 1 } sivyo { 2 })
f = unda(n) { kama (n > 2) { rudisha n - 2 } rudisha n + 2 }
andika(f(1), f(5), f(9223372036854775806))
andika(9223372036854775807 + 1)
andika(kama (1 < "x") { 1 } sivyo { 2 })
