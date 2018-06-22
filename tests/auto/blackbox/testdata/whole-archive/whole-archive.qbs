import qbs.Utilities

Project {
    StaticLibrary {
        name: "staticlib 1"
        Depends { name: "cpp" }
        files: ["unused1.cpp", "used.cpp"]
    }
    StaticLibrary {
        name: "staticlib2"
        Depends { name: "cpp" }
        files: ["unused2.cpp"]
    }
    StaticLibrary {
        name: "staticlib3"
        Depends { name: "cpp" }
        files: ["unused3.cpp"]
    }
    StaticLibrary {
        name: "staticlib4"
        Depends { name: "cpp" }
        files: ["unused4.cpp"]
    }

    DynamicLibrary {
        name: "dynamiclib"
        property string linkWholeArchive
        Depends { name: "cpp" }
        Probe {
            id: dummy
            property stringList toolchain: qbs.toolchain
            property string compilerVersion: cpp.compilerVersion
            property string dummy: product.linkWholeArchive // To force probe re-execution
            configure: {
                if (!toolchain.contains("msvc")
                        || Utilities.versionCompare(compilerVersion, "19.0.24215.1") >= 0) {
                    console.info("can link whole archives");
                } else {
                    console.info("cannot link whole archives");
                }
            }
        }
        Depends { name: "staticlib 1"; cpp.linkWholeArchive: product.linkWholeArchive }
        Depends { name: "staticlib2"; cpp.linkWholeArchive: product.linkWholeArchive }
        Depends { name: "staticlib3" }
        Depends { name: "staticlib4"; cpp.linkWholeArchive: product.linkWholeArchive }
        files: ["dynamiclib.cpp"]
    }

    CppApplication {
        name: "app1"
        Depends { name: "dynamiclib" }
        files: ["main1.cpp"]
    }

    CppApplication {
        name: "app2"
        Depends { name: "dynamiclib" }
        files: ["main2.cpp"]
    }
    CppApplication {
        name: "app3"
        Depends { name: "dynamiclib" }
        files: ["main3.cpp"]
    }
    CppApplication {
        name: "app4"
        Depends { name: "dynamiclib" }
        files: ["main4.cpp"]
    }
}
