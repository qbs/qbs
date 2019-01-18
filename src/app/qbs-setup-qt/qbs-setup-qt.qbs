import qbs 1.0

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
        files: ["qbs-setup-qt.exe.manifest", "qbs-setup-qt.rc"]
    }
}

