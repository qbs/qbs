import qbs 1.0

QbsApp {
    name: "qbs-setup-qt"
    Depends { name: "qbsqtprofilesetup" }
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
        files: ["qbs-setup-qt.rc"]
    }
}

