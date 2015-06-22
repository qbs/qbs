import qbs

StaticLibrary {
    name: "mylib"
    Depends { name: "dummy" }
    dummy.defines: ["BUILD_" + product.name.toUpperCase()]
    property string definePrefix: "USE_"
    Export {
        Depends { name: "dummy" }
        dummy.defines: [definePrefix + product.name.toUpperCase()]
        dummy.includePaths: ["./lib"]
    }
}
