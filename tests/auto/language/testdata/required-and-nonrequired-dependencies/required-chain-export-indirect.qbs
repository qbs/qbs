Project {
    Product {
        name: "dep2"
        Export {
            Depends { name: "dep1" }
        }
    }

    Product {
        name: "dep1"
        Export {
            Depends { name: "failing-validation-indirect" }
        }
    }

    Product {
        Depends { name: "failing-validation"; required: false }
        Depends { name: "dep2"; required: false }
    }
}
