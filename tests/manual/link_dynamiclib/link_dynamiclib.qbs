import qbs 1.0

Project {
    Application {
        name : "HelloWorld"
        destinationDirectory: "bin"
        Group {
            files : [ "main.cpp" ]
        }
        Depends { name: "cpp" }
        Depends { name: "lib1" }
        Depends { name: "lib4" }
    }

    DynamicLibrary {
        name : "lib1"
        destinationDirectory: "bin"
        Group {
            files : [ "lib1.cpp" ]
        }
        Depends { name: "cpp" }
        Depends { name: "lib2" }
    }

    DynamicLibrary {
        name : "lib2"
        destinationDirectory: "bin"
        cpp.visibility: 'default'
        Group {
            files : [ "lib2.cpp" ]
        }
        Depends { name: "cpp" }
        Depends { name: "lib3" }
    }

    DynamicLibrary {
        name : "lib3"
        destinationDirectory: "bin"
        cpp.visibility: 'hidden'
        Group {
            files : [ "lib3.cpp" ]
        }
        Depends { name: "cpp" }
    }

    DynamicLibrary {
        name : "lib4"
        destinationDirectory: "bin"
        cpp.visibility: 'hiddenInlines'
        cpp.defines: "TEST_LIB"
        Group {
            files : [ "lib4.h", "lib4.cpp" ]
        }
        Depends { name: "cpp" }

        Export {
            Depends { name: "cpp" }
            cpp.includePaths: ['.']
        }
    }
}

