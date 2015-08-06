import qbs

Product {
    Group {
        files: ["*.txt"]
        prefix: "**/"
        qbs.install: true
        qbs.installDir: "files"
    }
}
