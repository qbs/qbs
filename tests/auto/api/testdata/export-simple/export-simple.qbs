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
        Group {
            files: 'main.cpp'
            qbs.install: true
        }
        Export {
            Depends { name: 'dummy2' }
            Properties {    // QBS-550
                condition: false
                qbs.optimization: "ludicrous speed"
            }
        }
    }

    Product {
        name: 'dummy2'
        Group {
            files: 'lib1.cpp'
            qbs.install: true
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
        Properties {
            condition: qbs.targetOS.contains("darwin")
            bundle.isBundle: false
        }
    }
}

