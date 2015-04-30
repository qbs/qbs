import qbs 1.0

Product {
    Group {
        qbs.install: true
        qbs.installSourceBase: "."
        files: ["dir/**"]
    }
}
