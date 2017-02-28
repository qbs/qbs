import qbs

CppApplication {
    consoleApplication: true
    files: ["main.cpp"]
    Group {
        fileTagsFilter: ["application"]
        qbs.install: true
        qbs.installDir: "../whatever"
    }
}
