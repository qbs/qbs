Project {
    Product {
        name: "p1"
        Export {
            Depends { name: "cpp" }
            cpp.blubb: "x"
        }
    }
    Product {
        Depends { name: "p1" }
    }
}
