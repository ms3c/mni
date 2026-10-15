// pakeji: member scope, @, boundaries and property access
x = "global"
pakeji P {
    x = "package"
    getX = unda() { rudisha x }
    setX = unda(v) { x = v rudisha x }
    setNew = unda() { created = "made in package" rudisha created }
    self = unda() { rudisha @ }
    bump = unda() { @.count++ rudisha @.count }
    count = 0
}
andika(x, P.x, P.getX())
andika(P.setX("changed"), x, P.x)
andika(P.setNew(), P.created)
andika(P.self() == P, aina(P), aina(P.getX))
andika(P.bump(), P.bump(), P.count)
P.extra = 5
P.extra += 1
andika(P.extra)

// a function made inside a package keeps the package as its scope
pakeji R {
    make = unda() { rudisha unda() { rudisha @.tag } }
    tag = "R-tag"
}
andika(R.make()())

// module-level assignment does not touch an existing global from inside a package function
y = 1
pakeji S {
    touch = unda() { y = 2 rudisha y }
}
andika(S.touch(), y)

// packages nest, and @ is the innermost one
pakeji Outer {
    name = "outer"
    pakeji Inner {
        name = "inner"
        who = unda() { rudisha @.name }
    }
    who = unda() { rudisha @.name }
}
andika(Outer.who(), Outer.Inner.who())
