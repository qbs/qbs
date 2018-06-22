Project {
    Product {
        name: "v-bug"

        Export {
            Depends { name: "cpp"}
            cpp.defines: ""
        }
    }

    Product {
        name: "e-bug"

        Export { Depends { name: "v-bug" } }
    }

    Product {
        name: "u-bug"

        Export { Depends { name: "c-bug" } }
    }

    Product {
        name: "c-bug"

        Export { Depends { name: "e-bug" } }
    }

    Product
    {
        name: "H-bug"

        Depends { name: "e-bug" }
        Depends { name: "u-bug" }

        Group {
            qbs.install: true
        }
    }
}
