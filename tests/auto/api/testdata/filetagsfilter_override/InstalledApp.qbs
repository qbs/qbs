import qbs

CppApplication {
    type: "application"
    Group {
        fileTagsFilter: product.type
        qbs.install: true
        qbs.installDir: "hurz"
    }
}
