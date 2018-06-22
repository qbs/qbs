CppApplication {
    consoleApplication: true
    files: "main.cpp"
    Group {
        condition: false
        qbs.install: true
        fileTagsFilter: product.type
    }
}
