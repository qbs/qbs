Project {
    Product {
        name: "first_product"
        Depends { name: "buildenv" }
        Depends { name: "m" }
    }
    Product {
        name: "second_product"
        Depends { name: "m" }
    }
}
