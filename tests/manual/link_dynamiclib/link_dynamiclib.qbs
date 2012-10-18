import qbs.base 1.0

Project {
    Application {
        name : "HelloWorld"
        destination: "bin"
        Group {
            files : [ "main.cpp" ]
        }
        Depends { name: "cpp" }
        Depends { name: "lib1" }
        Depends { name: "lib4" }
    }

    DynamicLibrary {
        name : "lib1"
        destination: "bin"
        Group {
            files : [ "lib1.cpp" ]
        }
        Depends { name: "cpp" }
        Depends { name: "lib2" }
    }

    DynamicLibrary {
        name : "lib2"
        destination: "bin"
        cpp.visibility: 'default'
        Group {
            files : [ "lib2.cpp" ]
        }
        Depends { name: "cpp" }
        Depends { name: "lib3" }
    }

    DynamicLibrary {
        name : "lib3"
        destination: "bin"
        cpp.visibility: 'hidden'
        Group {
            files : [ "lib3.cpp" ]
        }
        Depends { name: "cpp" }
    }

    DynamicLibrary {
        name : "lib4"
        destination: "bin"
        cpp.visibility: 'hiddenInlines'
        cpp.defines: "TEST_LIB"
        Group {
            files : [ "lib4.h", "lib4.cpp" ]
        }
        Depends { name: "cpp" }

        ProductModule {
            Depends { name: "cpp" }
            cpp.includePaths: ['.']
        }
    }
}

