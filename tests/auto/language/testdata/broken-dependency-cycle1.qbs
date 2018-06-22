Project {
    Product {
        name: "p1"
        Export {
            property bool c: true
            Depends { name: "p2"; condition: c }
        }
    }
    Product {
        name: "p2"
        Depends { name: "p1" }
        p1.c: false
    }
    Product {
        name: "p3"
        Depends { name: "p1" }
    }
}
