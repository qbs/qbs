Product {
    qbs.installPrefix: ""
    Group {
        files: "dir/**"
        qbs.install: true
        qbs.installDir: "dir"
    }
}
