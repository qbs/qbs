Project {
    CppApplication {
        name: "app1"
        type: ["application"]
        files: ["main.cpp"]
        cpp.separateDebugInformation: true

        Probe {
            id: osProbe
            property stringList targetOS: qbs.targetOS
            configure: {
                console.info("is windows: " + (targetOS.contains("windows") ? "yes" : "no"));
                console.info("is macos: " + (targetOS.contains("macos") ? "yes" : "no"));
                console.info("is darwin: " + (targetOS.contains("darwin") ? "yes" : "no"));
            }
        }
    }
    DynamicLibrary {
        Depends { name: "cpp" }
        name: "foo1"
        type: ["dynamiclibrary"]
        files: ["foo.cpp"]
        cpp.separateDebugInformation: true
    }
    LoadableModule {
        Depends { name: "cpp" }
        name: "bar1"
        files: ["foo.cpp"]
        cpp.separateDebugInformation: true
    }

    CppApplication {
        name: "app2"
        type: ["application"]
        files: ["main.cpp"]
        cpp.separateDebugInformation: false
    }
    DynamicLibrary {
        Depends { name: "cpp" }
        name: "foo2"
        type: ["dynamiclibrary"]
        files: ["foo.cpp"]
        cpp.separateDebugInformation: false
    }
    LoadableModule {
        Depends { name: "cpp" }
        name: "bar2"
        files: ["foo.cpp"]
        cpp.separateDebugInformation: false
    }

    CppApplication {
        name: "app3"
        type: ["application"]
        files: ["main.cpp"]
        cpp.separateDebugInformation: true
        Properties {
            condition: qbs.targetOS.contains("darwin")
            cpp.dsymutilFlags: ["--flat"]
        }
    }
    DynamicLibrary {
        Depends { name: "cpp" }
        name: "foo3"
        type: ["dynamiclibrary"]
        files: ["foo.cpp"]
        cpp.separateDebugInformation: true
        Properties {
            condition: qbs.targetOS.contains("darwin")
            cpp.dsymutilFlags: ["--flat"]
        }
    }
    LoadableModule {
        Depends { name: "cpp" }
        name: "bar3"
        files: ["foo.cpp"]
        cpp.separateDebugInformation: true
        Properties {
            condition: qbs.targetOS.contains("darwin")
            cpp.dsymutilFlags: ["--flat"]
        }
    }

    CppApplication {
        name: "app4"
        type: ["application"]
        files: ["main.cpp"]
        consoleApplication: true
        cpp.separateDebugInformation: true
    }
    DynamicLibrary {
        Depends { name: "cpp" }
        name: "foo4"
        type: ["dynamiclibrary"]
        files: ["foo.cpp"]
        Properties {
            condition: qbs.targetOS.contains("darwin")
            bundle.isBundle: false
        }
        cpp.separateDebugInformation: true
    }
    LoadableModule {
        Depends { name: "cpp" }
        name: "bar4"
        files: ["foo.cpp"]
        Properties {
            condition: qbs.targetOS.contains("darwin")
            bundle.isBundle: false
        }
        cpp.separateDebugInformation: true
    }

    CppApplication {
        name: "app5"
        type: ["application"]
        files: ["main.cpp"]
        Properties {
            condition: qbs.targetOS.contains("darwin")
            bundle.isBundle: false
        }
        cpp.separateDebugInformation: true
        Properties {
            condition: qbs.targetOS.contains("darwin")
            cpp.dsymutilFlags: ["--flat"]
        }
    }
    DynamicLibrary {
        Depends { name: "cpp" }
        name: "foo5"
        type: ["dynamiclibrary"]
        files: ["foo.cpp"]
        Properties {
            condition: qbs.targetOS.contains("darwin")
            bundle.isBundle: false
        }
        cpp.separateDebugInformation: true
        Properties {
            condition: qbs.targetOS.contains("darwin")
            cpp.dsymutilFlags: ["--flat"]
        }
    }
    LoadableModule {
        Depends { name: "cpp" }
        name: "bar5"
        files: ["foo.cpp"]
        Properties {
            condition: qbs.targetOS.contains("darwin")
            bundle.isBundle: false
        }
        cpp.separateDebugInformation: true
        Properties {
            condition: qbs.targetOS.contains("darwin")
            cpp.dsymutilFlags: ["--flat"]
        }
    }
}
