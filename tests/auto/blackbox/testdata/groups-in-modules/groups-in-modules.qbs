import qbs

Project {
    Product {
        Depends { name: "dep" }
        Depends { name: "helper" }
        Depends {
            name: "helper3"
            required: false
        }
        type: ["diamond"]

        files: [
            "rock.coal"
        ]
    }

    Product {
        name: "dep"
        Export {
            Depends { name: "helper4" }
        }
    }
}
