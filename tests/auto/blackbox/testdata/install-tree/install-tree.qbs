import qbs

CppApplication {
    files: ["main.cpp"]
    Group {
        files: ["data/**/*.txt"]
        qbs.install: true
        qbs.installDir: "content"
        qbs.installSourceBase: "data"
    }
}
