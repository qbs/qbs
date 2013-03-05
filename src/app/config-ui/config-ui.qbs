import qbs 1.0
import "../apptemplate.qbs" as QbsApp

QbsApp {
    Depends { name: "qt.widgets" }
    name: "qbs-config-ui"
    files: [
        "main.cpp",
        "mainwindow.cpp",
        "mainwindow.h",
        "mainwindow.ui",
        "settingsmodel.cpp",
        "settingsmodel.h"
    ]
}

