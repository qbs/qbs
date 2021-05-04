CppApplication {
    consoleApplication: true
    files: "main.cpp"
    config.install.install: false
    Group {
        condition: false
        qbs.install: true
        fileTagsFilter: product.type
    }
}
