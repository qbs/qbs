Project {
    Product { name: "dep" }
    Product {
        name: "main"

        property bool enableGroup1
        property bool enableGroup2
        property bool enableDepends

        Group {
            condition: enableGroup1
            Group {
               condition: product.enableGroup2
               property bool forwarded: product.enableDepends
               Depends { name: "dep"; condition: forwarded }
            }
        }
    }
}
