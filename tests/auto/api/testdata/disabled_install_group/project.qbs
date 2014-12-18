import qbs

CppApplication {
    files: "main.cpp"
    Group {
        condition: false
        qbs.install: true
        fileTagsFilter: product.type
    }
    bundle.isBundle: false
}
