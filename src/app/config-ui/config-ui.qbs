import qbs 1.0
import "../apptemplate.qbs" as QbsApp

QbsApp {
    Depends { name: "Qt.widgets" }
    name: "qbs-config-ui"
    consoleApplication: false
    files: [
        "commandlineparser.cpp",
        "commandlineparser.h",
        "main.cpp",
        "mainwindow.cpp",
        "mainwindow.h",
        "mainwindow.ui",
        "settingsmodel.cpp",
        "settingsmodel.h"
    ]
}

