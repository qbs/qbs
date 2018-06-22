Project {
    Product {
        type: ["a"]
        name: "A"
        Depends { name: "B" }
    }
    Product {
        name: "B"
        Depends { productTypes: ["a"] }
    }
}
