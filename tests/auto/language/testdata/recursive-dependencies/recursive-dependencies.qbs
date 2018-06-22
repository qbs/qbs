Project {
    Product {
        name: "p1"
        Depends { name: "p3" }
    }
    Product {
        name: "p2"
        Depends { name: "p3" }
    }
    Product {
        name: "p3"
        Export {
            Depends { name: "p4" }
        }
    }
    Product { name: "p4" }
}
