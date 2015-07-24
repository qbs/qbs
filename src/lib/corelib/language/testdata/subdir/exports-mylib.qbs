import qbs

StaticLibrary {
    name: "mylib"
    Depends { name: "dummy" }
    dummy.defines: ["BUILD_" + product.name.toUpperCase()]
    property string definePrefix: "USE_"
    Export {
        Depends { name: "dummy" }
        Depends { name: "mylib2" }
        dummy.defines: [product.definePrefix + product.name.toUpperCase()]
        dummy.includePaths: ["./lib"]
    }
}
