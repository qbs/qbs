import qbs

Project {
    Product {
        Depends { name: "limerick" }
        type: ["text"]
        files: ["gerbil.txt.in"]
        Group {
            fileTagsFilter: product.type
            qbs.install: true
        }
    }
}
