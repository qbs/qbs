Project {
    Product {
        name: "dep"
        Export { Depends { name: "blubb" } }
    }
    Product { name: "p1"; Depends { name: "dep" } }
    Product { name: "p2"; Depends { name: "dep" } }
}
