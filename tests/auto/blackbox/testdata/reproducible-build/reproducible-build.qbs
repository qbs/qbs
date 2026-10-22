Project {
    CppApplication {
        name: "the product"
        consoleApplication: true
        property bool dummy: { console.info("executable suffix: " + cpp.executableSuffix); }
        Depends { name: "dep" }
        cpp.cxxFlags: ["-flto"]
        files: ["main.cpp"]
        Probe {
            id: checker
            property bool isGcc: qbs.toolchain.contains("gcc") && !qbs.toolchain.contains("clang")
            property string objectSuffix: cpp.objectSuffix
            configure: {
                console.info("is gcc: " + isGcc);
                console.info("object suffix: " + objectSuffix);
            }
        }
    }
    ObjectLibrary {
        name: "dep"
        cpp.cxxFlags: ["-flto"]
        files: [
            "file1.cpp", "file2.cpp", "file3.cpp", "file4.cpp", "file5.cpp", "file6.cpp",
            "file7.cpp"]
    }
    CppApplication {
        name: "randomizer"
        files: [
            "file1.cpp", "file2.cpp", "file3.cpp", "file4.cpp", "file5.cpp", "file6.cpp",
            "file7.cpp", "main.cpp"]
    }
}
