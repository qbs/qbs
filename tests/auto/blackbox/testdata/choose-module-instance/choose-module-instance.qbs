import qbs
import qbs.FileInfo

Project {
    qbsSearchPaths: [".", path, "modules/..", FileInfo.path(FileInfo.joinPaths(path, "modules"))]
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
