CppApplication {
    name: "the product"
    files: ["file1.cpp", "file2.cpp", "main.cpp"]
    cpp.cxxFlags: ["-flto"]
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
