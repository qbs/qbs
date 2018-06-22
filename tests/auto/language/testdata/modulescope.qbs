import "modulescope_base.qbs" as MyProduct

Project {
    MyProduct {
        name: "product1"
        property int e: 12
        property int f: 13
        scopemod.a: 2
        scopemod.f: 2
        scopemod.g: e * f
        scopemod.h: base + 2
    }
}
