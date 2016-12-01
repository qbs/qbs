Product {
    Export {
        Depends { name: "dummy" }
        dummy.cFlags: ["BASE_" + product.name.toUpperCase()]
        dummy.cxxFlags: ["-foo"]
        dummy.defines: ["ABC"]
    }
}
