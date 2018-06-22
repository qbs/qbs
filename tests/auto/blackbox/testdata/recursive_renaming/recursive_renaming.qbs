Product {
    qbs.installPrefix: ""
    Group {
        qbs.install: true
        qbs.installSourceBase: "."
        files: ["dir/**"]
    }
}
