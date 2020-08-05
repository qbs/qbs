Project {
    Product {
        name: "p"
        Depends { name: "dep" }
    }
    Product {
        name: "dep"
        Export { Depends { name: "m" } }
    }
}
