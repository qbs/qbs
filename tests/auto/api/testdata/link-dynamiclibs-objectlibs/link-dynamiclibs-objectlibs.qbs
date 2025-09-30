Project {
    Application {
        name : "HelloWorld"
        files : [ "main.cpp" ]
        Depends { name: "cpp" }
        Depends { name: "dynamic1" }
    }

    DynamicLibrary {
        name : "dynamic1"
        files : [ "dynamic1.cpp" ]
        Depends { name: "cpp" }
        Depends { name: "object1" }
        Properties {
            condition: qbs.targetOS.includes("darwin")
            bundle.isBundle: false
        }
    }

    ObjectLibrary {
        name: "object1"
        files: [ "object1.cpp" ]
        Depends { name: "cpp" }
        Depends { name: "dynamic2" }
    }

    DynamicLibrary {
        name: "dynamic2"
        files: [ "dynamic2.cpp" ]
        Depends { name: "cpp" }
        Depends { name: "object2" }
        cpp.visibility: 'hidden'
        Properties {
            condition: qbs.targetOS.includes("darwin")
            bundle.isBundle: false
        }
    }

    ObjectLibrary {
        name: "object2"
        files: [ "object2.cpp", "object2.h" ]
        Depends { name: "cpp" }
        Probe {
            id: tcPrinter
            property bool isGcc: qbs.toolchain.contains("gcc")
            property bool isEmscripten: qbs.toolchain.contains("emscripten")
            configure: {
                console.info("is gcc: " + isGcc);
                console.info("is emscripten: " + isEmscripten);
            }
        }
    }
}

