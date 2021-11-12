Project {
    CppApplication {
        name: "app"
        Depends { name: "lib" }
        property bool dummy: {
            console.info("is GCC: " + qbs.toolchain.contains("gcc"));
            console.info("is MinGW: " + qbs.toolchain.contains("mingw"));
            console.info("is Darwin: " + qbs.targetOS.contains("darwin"));
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
