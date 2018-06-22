CppApplication {
    files: ["main.cpp"]
    qbs.installPrefix: ""
    Group {
        files: ["data/**/*.txt"]
        qbs.install: true
        qbs.installDir: "content"
        qbs.installSourceBase: "data"
    }
}
