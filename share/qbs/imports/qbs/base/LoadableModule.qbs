import qbs

Product {
    Depends { name: "bundle" }
    type: qbs.targetOS.contains("darwin") ? ["loadablemodule"] : ["dynamiclibrary"]
}
