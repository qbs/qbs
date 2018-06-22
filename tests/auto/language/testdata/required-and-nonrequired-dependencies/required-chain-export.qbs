Project {
    Product {
        name: "dep"
        Export {
            Depends { name: "failing-validation" }
        }
    }

    Product {
        Depends { name: "failing-validation"; required: false }
        Depends { name: "dep"; required: false }
    }
}
