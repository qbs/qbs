Product {
    name: "span"
    files: [
        "LICENSE_1_0.txt",
        "README.md",
        "span.hpp",
    ]
    Export {
        Depends { name: "cpp" }
        cpp.includePaths: exportingProduct.sourceDirectory
    }
}
