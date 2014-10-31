import qbs 1.0

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
    ]

    cpp.infoPlistFile: "Info.plist"
}
