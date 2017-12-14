import qbs

CppApplication {
    Depends { name: "Qt.quick" }

    cpp.cxxLanguageVersion: "c++11"

    Probe {
        id: qtQuickCompilerProbe
        property bool hasCompiler: Qt.quick.compilerAvailable
        configure: {
            if (hasCompiler)
                console.info("compiler available");
            else
                console.info("compiler not available");
        }
    }

    files: [
        "main.cpp",
        "qml.qrc",
    ]
}
