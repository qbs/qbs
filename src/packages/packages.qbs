import qbs

Project {
    references: [
        "archive/archive.qbs",
        "chocolatey/chocolatey.qbs",
    ]

    // Virtual product for building all possible packagings
    Product {
        Depends { name: "qbs archive"; required: false }
        Depends { name: "qbs chocolatey"; required: false }
        name: "dist"
        builtByDefault: false
    }
}
