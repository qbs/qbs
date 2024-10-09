CppApplication {
    Depends { name: "Qt.widgets" }
    consoleApplication: true
    property bool dummy: { console.info("is emscripten: " + qbs.toolchain.includes("emscripten")); }
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

