import qbs 1.0
import "../apptemplate.qbs" as QbsApp

QbsApp {
    Depends { name: "Qt.widgets" }
    name: "qbs-config-ui"
    consoleApplication: false
    files: [
        "main.cpp",
        "mainwindow.cpp",
        "mainwindow.h",
        "mainwindow.ui",
        "settingsmodel.cpp",
        "settingsmodel.h"
    ]
}

