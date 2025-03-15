//![1]
// myproject.qbs
CppApplication {
    condition: {
        if (qbs.toolchainType === "msvc"
            || ((qbs.toolchainType === "gcc")
                && cpp.compilerVersionMajor >= 15)
            || (qbs.toolchainType === "clang" && cpp.compilerVersionMajor >= 18)) {
            return true;
        }
        console.info("Unsupported toolchainType " + qbs.toolchainType);
        return false;
    }
    consoleApplication: true
    install: true
    files: ["main.cpp" ]
    //![0]
    cpp.cxxLanguageVersion: "c++23"
    cpp.forceUseCxxModules: true
    cpp.forceUseImportStd: true
    //![0]
    Properties {
        condition: qbs.toolchainType === "clang"
        cpp.cxxFlags: ["-Wno-reserved-module-identifier"]
        cpp.cxxStandardLibrary: "libc++"
    }
}
//![1]
