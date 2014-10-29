import qbs 1.0

QbsApp {
    Depends { name: "Qt.widgets" }
    name: "qbs-config-ui"
    consoleApplication: false
    files: [
        "../shared/qbssettings.cpp",
        "../shared/qbssettings.h",
        "commandlineparser.cpp",
        "commandlineparser.h",
        "main.cpp",
        "mainwindow.cpp",
        "mainwindow.h",
        "mainwindow.ui",
        "settingsmodel.cpp",
        "settingsmodel.h"
    ]

    cpp.infoPlistFile: "Info.plist"
}
