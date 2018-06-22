Product {
    name: "p"
    property string installRoot
    qbs.installRoot: installRoot
    Group {
        qbs.install: true
        qbs.installPrefix: "/install-prefix"
        qbs.installDir: "/install-dir"
        files: ["file.txt"]
    }
}
