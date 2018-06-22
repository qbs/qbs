Project {
    Application {
        name : "HelloWorld"
        Group {
            files : [ "main.cpp" ]
        }
        Depends { name: "cpp" }
        Depends { name: "mylib" }
    }

    DynamicLibrary {
        name : "mylib"
        version: "1.2.3"
        Group {
            files : [ "mylib.cpp" ]
        }
        Depends { name: "cpp" }
        cpp.defines: ["XXXX"]

        Properties {
            condition: qbs.targetOS.contains("darwin")
            bundle.isBundle: false
        }
    }
}

