StaticLibrary {
    name: "mylib2"
    Depends { name: "dummy" }
    dummy.defines: ["BUILD_" + product.name.toUpperCase()]
    property string definePrefix: "USE_"
    Export {
        Depends { name: "dummy" }
        dummy.defines: [product.definePrefix + product.name.toUpperCase()]
        dummy.includePaths: ["./lib"]
    }
}
