Project {
    Application {
        name : "HelloWorld"
        Group {
            files : [ "main.cpp" ]
        }
        Depends { name: "cpp" }
        Depends { name: "lib1" }
        Depends { name: "lib4" }
    }

    DynamicLibrary {
        name : "lib1"
        Group {
            files : [ "lib1.cpp" ]
        }
        Depends { name: "cpp" }
        Depends { name: "lib2" }
        Properties {
            condition: qbs.targetOS.contains("darwin")
            bundle.isBundle: false
        }
    }

    DynamicLibrary {
        name : "lib2"
        cpp.visibility: 'default'
        Group {
            files : [ "lib2.cpp" ]
        }
        Depends { name: "cpp" }
        Depends { name: "lib3" }
        Properties {
            condition: qbs.targetOS.contains("darwin")
            bundle.isBundle: false
        }
    }

    DynamicLibrary {
        name : "lib3"
        cpp.visibility: 'hidden'
        Group {
            files : [ "lib3.cpp" ]
        }
        Depends { name: "cpp" }
        Properties {
            condition: qbs.targetOS.contains("darwin")
            bundle.isBundle: false
        }
    }

    DynamicLibrary {
        name : "lib4"
        cpp.visibility: 'hiddenInlines'
        cpp.defines: "TEST_LIB"
        Group {
            files : [ "lib4.h", "lib4.cpp" ]
        }
        Depends { name: "cpp" }
        Properties {
            condition: qbs.targetOS.contains("darwin")
            bundle.isBundle: false
        }

        Export {
            Depends { name: "cpp" }
            cpp.includePaths: [product.sourceDirectory]
        }
    }
}

