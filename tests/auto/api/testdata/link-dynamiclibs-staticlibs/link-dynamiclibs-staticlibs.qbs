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
        Depends { name: "static1" }
        Properties {
            condition: qbs.targetOS.contains("darwin")
            bundle.isBundle: false
        }
    }

    StaticLibrary {
        name: "static1"
        files: [ "static1.cpp" ]
        Depends { name: "cpp" }
        Depends { name: "dynamic2" }
    }

    DynamicLibrary {
        name: "dynamic2"
        files: [ "dynamic2.cpp" ]
        Depends { name: "cpp" }
        Depends { name: "static2" }
        cpp.visibility: 'hidden'
        Properties {
            condition: qbs.targetOS.contains("darwin")
            bundle.isBundle: false
        }
    }

    StaticLibrary {
        name: "static2"
        files: [ "static2.cpp", "static2.h" ]
        Depends { name: "cpp" }
    }
}

