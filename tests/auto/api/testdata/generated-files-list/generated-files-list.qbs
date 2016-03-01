import qbs

CppApplication {
    Depends { name: "Qt.widgets" }
    files: [
        "main.cpp",
        "mainwindow.cpp",
        "mainwindow.h",
        "mainwindow.ui"
    ]
}

