Project {
    CppApplication {
        consoleApplication: true
        name: "LinkedProduct-Assembly"
        property bool dummy: {
            console.info("is emscripten: " + qbs.toolchain.includes("emscripten"));
        }
        files: qbs.targetOS.includes("darwin") ? "darwin.s" : "main.s"
        installDir: ""

        cpp.linkerPath: cpp.compilerPathByLanguage["c"]
    }

    CppApplication {
        consoleApplication: true
        name: "LinkedProduct-C"
        files: ["main.c"]
        installDir: ""
    }

    CppApplication {
        condition: qbs.targetOS.includes("darwin")

        consoleApplication: true
        name: "LinkedProduct-Objective-C"
        files: ["main.m"]
        installDir: ""
        cpp.dynamicLibraries: ["ObjC"]
    }

    CppApplication {
        consoleApplication: true
        name: "LinkedProduct-C++"
        files: ["main.cpp"]
        installDir: ""
    }

    CppApplication {
        condition: qbs.targetOS.includes("darwin")

        consoleApplication: true
        name: "LinkedProduct-Objective-C++"
        files: ["main.mm"]
        installDir: ""

        cpp.dynamicLibraries: ["ObjC"]
    }

    CppApplication {
        Depends { name: "LinkedProduct-C++StaticLibrary" }

        name: "LinkedProduct-BlankApp"
        files: ["staticmain.c"]
        installDir: ""
    }

    StaticLibrary {
        Depends { name: "cpp" }

        name: "LinkedProduct-C++StaticLibrary"
        files: ["staticlib.cpp"]
        installDir: ""
    }
}
