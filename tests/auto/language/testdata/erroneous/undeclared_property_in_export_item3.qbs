Project {
    Product {
        name: "p1"
        Export { blubb: false }
    }
    Product { Depends { name: "p1" } }
}
