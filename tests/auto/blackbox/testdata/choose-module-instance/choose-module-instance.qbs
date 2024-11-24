import qbs.FileInfo

Project {
    qbsSearchPaths: [
        ".", path, "modules/..", FileInfo.path(FileInfo.joinPaths(path, "modules")),
        "other-searchpath"
    ]
    Product {
        Depends { name: "limerick"; versionAtLeast: "2" }
        type: ["text"]
        files: ["gerbil.txt.in"]
        Group {
            fileTagsFilter: product.type
            qbs.install: true
        }
    }
}
