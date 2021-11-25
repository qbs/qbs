Project {
    Product {
        name: "importing"
        Depends { name: "dep" }
    }
    Product {
        name: "dep"
        Export { Depends { name: "m" } }
    }
}
