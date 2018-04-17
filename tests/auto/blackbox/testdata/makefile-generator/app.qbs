import qbs

CppApplication {
    name: "the app"
    consoleApplication: true
    files: "main.cpp"
    qbs.installPrefix: "/usr/local"
    Group {
        fileTagsFilter: "application"
        qbs.install: true
        qbs.installDir: "bin"
    }
}
