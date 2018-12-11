Product {
    name: "p"
    Depends { name: "module_with_file" }
    property bool addFileToProduct
    Group {
        files: "zort"
        condition: addFileToProduct
    }
}
