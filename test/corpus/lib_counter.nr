// a package file used by imports.nr
andika("lib_counter loads")
secret = "module-level, private to this file"
pakeji lib_counter {
    n = 0
    next = unda() { @.n++ rudisha @.n }
    peek = unda() { rudisha secret }
}
