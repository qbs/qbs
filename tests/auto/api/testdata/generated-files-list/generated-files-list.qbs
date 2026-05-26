import qbs.Utilities

CppApplication {
    Depends { name: "Qt.widgets" }
    consoleApplication: true
    property bool dummy: {
        var isEmScripten = qbs.toolchain.includes("emscripten");
        console.info("is emscripten: " + isEmScripten);
        if (isEmScripten) {
            console.info("generates worker.js: "
                         + (Utilities.versionCompare(cpp.compilerVersion, "3.1.68") < 0))
        }
    }
    cpp.cxxLanguageVersion: "c++11"
    cpp.debugInformation: false
    cpp.separateDebugInformation: false
    files: [
        "main.cpp",
        "mainwindow.cpp",
        "mainwindow.h",
        "mainwindow.ui"
    ]
    Depends { name: "bundle" }
    bundle.isBundle: false
}
