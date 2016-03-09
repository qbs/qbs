import qbs

CppApplication {
    Depends { name: "Qt.widgets" }
    consoleApplication: true
    cpp.debugInformation: false
    cpp.separateDebugInformation: false
    files: [
        "main.cpp",
        "mainwindow.cpp",
        "mainwindow.h",
        "mainwindow.ui"
    ]
}

