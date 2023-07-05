Project {
    Product {
        name: "B"
        Depends { productTypes: ["a"] }
    }
    Product {
        type: ["a"]
        name: "A"
        Depends { name: "B" }
    }
}
