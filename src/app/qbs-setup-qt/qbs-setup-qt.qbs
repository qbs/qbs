import qbs 1.0
import "../apptemplate.qbs" as QbsApp

QbsApp {
    name: "qbs-setup-qt"
    files: [
        "../shared/qbssettings.h",
        "main.cpp",
        "setupqt.cpp",
        "setupqt.h"
    ]
}

