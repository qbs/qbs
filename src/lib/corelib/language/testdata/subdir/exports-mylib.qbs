import qbs

StaticLibrary {
    name: "mylib"
    Depends { name: "dummy" }
    dummy.defines: ["BUILD_" + product.name.toUpperCase()]
    Export {
        Depends { name: "dummy" }
        dummy.defines: ["USE_" + product.name.toUpperCase()]
        dummy.includePaths: ["./lib"]
    }
}
