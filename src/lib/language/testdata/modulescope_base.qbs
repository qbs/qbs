import qbs 1.0

Product {
    Depends { name: "scopemod" }
    scopemod.h: e * f
}
