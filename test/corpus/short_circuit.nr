// && and || always give a boolean and short-circuit
calls = []
t = unda(tag, v) { calls.sukuma(tag) rudisha v }
andika(1 && 2, tupu || 5, 0 && "x", sikweli || tupu)
andika(t("a", kweli) && t("b", sikweli) && t("c", kweli), calls)
calls = []
andika(t("a", 1) || t("b", 2), calls)
calls = []
andika(t("a", tupu) || t("b", sikweli) || t("c", "yes"), calls)
andika(!0, !tupu, !"", ![], !{}, !sikweli)
r = kweli && kama (kweli) { 5 } sivyo { 6 }
andika(r)
