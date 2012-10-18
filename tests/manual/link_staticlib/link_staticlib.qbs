import qbs.base 1.0

Project {
    Application {
        name: "HelloWorld"
        files : [ "main.cpp" ]
        Depends { name: "cpp" }
        Depends { name: "mystaticlib" }
    }

    StaticLibrary {
        name : "mystaticlib"
        files : [ "mystaticlib.cpp" ]
        Depends { name: "cpp" }
        Depends { name: "helperlib" }
    }

    StaticLibrary {
        name : "helperlib"
        files : [
            "helper/helper.h",
            "helper/helper.cpp"
        ]
        Depends { name: "cpp" }
        ProductModule {
            Depends { name: "cpp" }
            cpp.includePaths: ['helper']
        }
    }
}

