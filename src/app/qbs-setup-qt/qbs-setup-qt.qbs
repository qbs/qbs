import qbs 1.0
import "../apptemplate.qbs" as QbsApp

QbsApp {
    name: "qbs-setup-qt"
    Depends { name: "qbsqtprofilesetup" }
    files: [
        "../shared/qbssettings.h",
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

