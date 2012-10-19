Project {
    Product {
        name: "MyProduct"
        Group {
            name: product.name + ".MyGroup"
            files: "*"
        }
    }

    Product {
        name: "My2ndProduct"
        Group {
            name: product.name + ".MyGroup"
            files: ["narf"]
        }
        Group {
            files: ["zort"]
        }
    }
}
