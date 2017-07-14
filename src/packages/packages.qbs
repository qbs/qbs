import qbs

Project {
    references: [
        "archive/archive.qbs",
    ]

    // Virtual product for building all possible packagings
    Product {
        Depends { name: "qbs archive"; required: false }
        name: "dist"

        Group {
            name: "Scripts"
            prefix: "../../scripts/"
            files: [
                "make-release-archive.sh",
                "make-release-archives.bat",
            ]
        }
    }
}
