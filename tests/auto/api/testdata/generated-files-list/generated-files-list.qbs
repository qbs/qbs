CppApplication {
    Depends { name: "Qt.widgets" }
    consoleApplication: true
    cpp.cxxLanguageVersion: "c++11"
    cpp.debugInformation: false
    cpp.separateDebugInformation: false
    files: [
        "main.cpp",
        "mainwindow.cpp",
        "mainwindow.h",
        "mainwindow.ui"
    ]
}

