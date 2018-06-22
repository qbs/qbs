CppApplication {
    consoleApplication: true
    Group {
        fileTagsFilter: "application"
        qbs.install:true
        qbs.installPrefix: product.name
        qbs.installDir: "wrong"
    }
}
