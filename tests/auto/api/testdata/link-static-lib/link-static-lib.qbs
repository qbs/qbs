Project {
    Product {
        type: "application"
        consoleApplication: true
        name: "HelloWorld"
        files : [ "main.cpp" ]
        Depends { name: "cpp" }
        Depends { name: "mystaticlib" }
    }

    StaticLibrary {
        name : "mystaticlib"
        files : [ "mystaticlib.cpp" ]
        Depends { name: "cpp" }
        Depends { name: "helper1" }
    }

    StaticLibrary {
        name : "helper1"
        files : [
            "helper1/helper1.h",
            "helper1/helper1.cpp"
        ]
        Depends { name: "cpp" }
        Depends { name: "helper2" }
        Export {
            Depends { name: "cpp" }
            cpp.includePaths: [product.sourceDirectory + '/helper1']
        }
    }

    StaticLibrary {
        name : "helper2"
        files : [
            "helper2/helper2.h",
            "helper2/helper2.cpp"
        ]
        Depends { name: "cpp" }
        Export {
            Depends { name: "cpp" }
            cpp.includePaths: [product.sourceDirectory + '/helper2']
        }
    }
}

