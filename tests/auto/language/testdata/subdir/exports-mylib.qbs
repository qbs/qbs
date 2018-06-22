StaticLibrary {
    name: "mylib"
    Depends { name: "dummy" }
    dummy.defines: ["BUILD_" + product.name.toUpperCase()]
    property string definePrefix: "USE_"
    property path aPath: "."
    dummy.somePath: aPath
    Export {
        Depends { name: "dummy" }
        Depends { name: "mylib2" }
        dummy.defines: [product.definePrefix + product.name.toUpperCase()]
        dummy.includePaths: ["./lib"]
        dummy.somePath: product.aPath
    }
}
