Project {
    Product {
        name: "dep"
        Export {
            Depends { name: "dummy" }
            Group {
                name: "exported_group"
                dummy.someString: "test"
                files: ["narf"]
            }
        }
    }
    Product {
        Depends { name: "dep" }
    }
}
