import qbs

Project {
    qbsSearchPaths: ["./foo", "./bar"]
    references: ["product.qbs"]
}
