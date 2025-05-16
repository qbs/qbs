Project {
    Application {
        name : "HelloWorld"
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
        Group {
            files : [ "lib1.cpp" ]
        }
        Depends { name: "cpp" }
        Properties {
            condition: qbs.targetOS.includes("darwin")
            bundle.isBundle: false
        }
    }
}

