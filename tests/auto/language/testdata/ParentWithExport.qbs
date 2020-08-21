Product {
    Export {
        Depends { name: "dummy" }
        dummy.defines: [exportingProduct.name.toUpperCase()]
    }
}
