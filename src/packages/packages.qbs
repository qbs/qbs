Project {
    references: [
        "archive/archive.qbs",
        "chocolatey/chocolatey.qbs",
    ]

    // Virtual product for building all possible packagings
    Product {
        Depends { name: "qbs_archive"; required: false }
        Depends { name: "qbs chocolatey"; required: false }
        name: "dist"
        builtByDefault: false
    }
}
