CppApplication {
    consoleApplication: true
    files: "main.cpp"
    install: false
    Group {
        condition: false
        qbs.install: true
        fileTagsFilter: product.type
    }
}
