Project {
    Product {
        name: "dep"
        Export { Depends { name: "higher" } }
    }

    Product {
        name: "egon"
        Depends { name: "dep" }
        lower.prop1: "blubb"
    }
}
