Project {
    CppApplication {
        name: "app"
        Depends { name: "lib" }
        property bool dummy: {
            var isGCC = qbs.toolchain.includes("gcc") && !qbs.toolchain.includes("emscripten")
            console.info("is GCC: " + isGCC);
            console.info("is MinGW: " + qbs.toolchain.includes("mingw"));
            console.info("is Darwin: " + qbs.targetOS.includes("darwin"));
            console.info("is emscripten: " + qbs.toolchain.contains("emscripten"));
        }
        files: "main.cpp"
    }

    DynamicLibrary {
        name: "lib"
        Depends { name: "Qt.core" }

        cpp.cxxLanguageVersion: "c++11"
        cpp.defines: "SYMBOLSTEST_LIBRARY"

        files: [
            "lib.cpp",
            "lib.h",
        ]

        Export { Depends { name: "Qt.core" } }
    }
}
