QbsProduct {
    type: "staticlibrary"
    Export {
        Depends { name: "cpp" }
        Depends { name: "Qt"; submodules: ["core"] }

        cpp.includePaths: [exportingProduct.sourceDirectory]
    }
}
