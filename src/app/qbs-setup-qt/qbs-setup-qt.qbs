QbsApp {
    name: "qbs-setup-qt"
    files: [
        "commandlineparser.cpp",
        "commandlineparser.h",
        "main.cpp",
        "setupqt.cpp",
        "setupqt.h"
    ]
    Group {
        name: "MinGW specific files"
        condition: qbs.toolchain.contains("mingw")
        files: "qbs-setup-qt.rc"
        Group {
            name: "qbs-setup-qt manifest"
            files: "qbs-setup-qt.exe.manifest"
            fileTags: [] // the manifest is referenced by the rc file
        }
    }
}

