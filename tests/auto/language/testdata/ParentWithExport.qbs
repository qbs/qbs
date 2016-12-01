Product {
    Export {
        Depends { name: "dummy" }
        dummy.defines: [product.name.toUpperCase()]
    }
}
