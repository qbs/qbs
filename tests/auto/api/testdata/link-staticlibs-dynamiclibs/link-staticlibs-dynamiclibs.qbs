Project {
    Application {
        name : "HelloWorld"
        files : [ "main.cpp" ]
        Depends { name: "cpp" }
        Depends { name: "static1" }
    }

    StaticLibrary {
        name: "static1"
        files: [ "static1.cpp" ]
        Depends { name: "cpp" }
        Depends { name: "dynamic1" }

        Probe {
            id: osCheck
            property bool isNormalUnix: qbs.targetOS.includes("unix")
                                        && !qbs.targetOS.includes("darwin")
            property bool isGcc: qbs.toolchain.contains("gcc")
            configure: {
                console.info("is normal unix: " + (isNormalUnix ? "yes" : "no"));
                console.info("is gcc: " + isGcc);
            }
        }
    }

    DynamicLibrary {
        name : "dynamic1"
        files : [ "dynamic1.cpp" ]
        Depends { name: "cpp" }
        Depends { name: "static2" }
        Properties {
            condition: qbs.targetOS.includes("darwin")
            bundle.isBundle: false
        }
    }

    StaticLibrary {
        name: "static2"
        files: [ "static2.cpp", "static2.h" ]
        Depends { name: "cpp" }
        Depends { name: "dynamic2" }
    }

    DynamicLibrary {
        name: "dynamic2"
        files: [ "dynamic2.cpp" ]
        Depends { name: "cpp" }
        cpp.visibility: 'hidden'
        Properties {
            condition: qbs.targetOS.includes("darwin")
            bundle.isBundle: false
        }
    }
}

