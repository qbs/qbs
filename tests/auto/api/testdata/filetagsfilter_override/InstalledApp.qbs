CppApplication {
    type: "application"
    consoleApplication: true
    Group {
        fileTagsFilter: product.type
        qbs.install: true
        qbs.installDir: "hurz"
    }
}
