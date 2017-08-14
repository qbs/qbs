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
        condition: qbs.targetOS.contains("macos")
        files: [
            "fgapp.mm",
            "Info.plist"
        ]
    }

    Properties {
        condition: qbs.targetOS.contains("macos")
        cpp.frameworks: ["ApplicationServices", "Cocoa"]
    }

    Properties {
        condition: qbs.targetOS.contains("darwin")
        bundle.isBundle: false
    }
}
