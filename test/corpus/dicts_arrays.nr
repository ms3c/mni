// arrays and dictionaries
a = [1, [2, 3], "x"]
andika(a, a[1][0], a[-1], a[5], a.idadi(), a.yamwisho(), [].yamwisho())
a[0] = "first"
a[-1] = "last"
andika(a)
b = a
b.sukuma("shared")
andika(a, a == b)
c = a + [9]
andika(c, a)
andika([1, 2, 3].map(unda(x) { rudisha x * x }), [1, 2, 3, 4].chuja(unda(x) { rudisha x % 2 == 0 }))
d = {1: "int", 1.5: "float", kweli: "bool", "s": "string"}
andika(d[1], d[1.5], d[kweli], d["s"], d["none"], 1 ktk d, "s" ktk d, 2 ktk d)
d["new"] = [1]
d["new"].sukuma(2)
andika(d["new"])
e = {"b": 1, "a": 2}
e["c"] = 3
e["b"] = 10
andika(e)
f = {"x": 1} + {"y": 2} + {"x": 3}
andika(f)
nested = {"list": [1, {"deep": "value"}]}
andika(nested["list"][1]["deep"])
