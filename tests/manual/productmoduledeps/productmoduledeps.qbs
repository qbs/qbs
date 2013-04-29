import qbs 1.0

Project {
    Application {
        name : "HelloWorld"
        destinationDirectory: "bin"
        Group {
            files : [ "main.cpp" ]
        }
        Depends { name: "cpp" }
        Depends { name: 'dummy' }
    }

    Product {
        name: 'dummy'
        type: 'installed_content'
        Group {
            files: 'main.cpp'
            fileTags: 'install'
        }
        Export {
            Depends { name: 'dummy2' }
        }
    }

    Product {
        name: 'dummy2'
        type: 'installed_content'
        Group {
            files: 'lib1.cpp'
            fileTags: 'install'
        }
        Export {
            Depends { name: 'lib1' }
        }
    }

    DynamicLibrary {
        name : "lib1"
        destinationDirectory: "bin"
        Group {
            files : [ "lib1.cpp" ]
        }
        Depends { name: "cpp" }
    }
}

