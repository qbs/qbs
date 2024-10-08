Project {
    CppApplication {
        consoleApplication: true
        name: "LinkedProduct-Assembly"
        property bool dummy: {
            console.info("is emscripten: " + qbs.toolchain.includes("emscripten"));
        }
        files: qbs.targetOS.includes("darwin") ? "darwin.s" : "main.s"

        cpp.linkerPath: cpp.compilerPathByLanguage["c"]

        Group {
            fileTagsFilter: product.type
            qbs.install: true
        }
    }

    CppApplication {
        consoleApplication: true
        name: "LinkedProduct-C"
        files: ["main.c"]

        Group {
            fileTagsFilter: product.type
            qbs.install: true
        }
    }

    CppApplication {
        condition: qbs.targetOS.includes("darwin")

        consoleApplication: true
        name: "LinkedProduct-Objective-C"
        files: ["main.m"]

        cpp.dynamicLibraries: ["ObjC"]

        Group {
            fileTagsFilter: product.type
            qbs.install: true
        }
    }

    CppApplication {
        consoleApplication: true
        name: "LinkedProduct-C++"
        files: ["main.cpp"]

        Group {
            fileTagsFilter: product.type
            qbs.install: true
        }
    }

    CppApplication {
        condition: qbs.targetOS.includes("darwin")

        consoleApplication: true
        name: "LinkedProduct-Objective-C++"
        files: ["main.mm"]

        cpp.dynamicLibraries: ["ObjC"]

        Group {
            fileTagsFilter: product.type
            qbs.install: true
        }
    }

    CppApplication {
        Depends { name: "LinkedProduct-C++StaticLibrary" }

        name: "LinkedProduct-BlankApp"
        files: ["staticmain.c"]
    }

    StaticLibrary {
        Depends { name: "cpp" }

        name: "LinkedProduct-C++StaticLibrary"
        files: ["staticlib.cpp"]
    }
}
