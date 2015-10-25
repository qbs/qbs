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

    Group {
        condition: qbs.targetOS.contains("osx")
        files: ["fgapp.mm"]
    }

    Properties {
        condition: qbs.targetOS.contains("osx")
        cpp.frameworks: ["ApplicationServices", "Cocoa"]
    }

    Depends { name: "bundle" }
    bundle.isBundle: false
    bundle.infoPlistFile: "Info.plist"
}
