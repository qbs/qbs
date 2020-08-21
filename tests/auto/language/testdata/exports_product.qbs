Product {
    Export {
        version: "2.0"
        Depends { name: "dummy" }
        dummy.cFlags: ["BASE_" + exportingProduct.name.toUpperCase()]
        dummy.cxxFlags: ["-foo"]
        dummy.defines: ["ABC"]
    }
}
