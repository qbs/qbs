import qbs 1.0

Product {
    qbs.installPrefix: ""
    Group {
        qbs.install: true
        files: "*"
    }
}
