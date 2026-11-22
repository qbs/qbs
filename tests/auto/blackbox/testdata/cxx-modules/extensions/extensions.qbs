CppApplication {
    name: "extensions"
    condition: {
        if (qbs.toolchainType === "msvc"
            || ((qbs.toolchainType === "gcc" || qbs.toolchainType === "mingw")
                && cpp.compilerVersionMajor >= 11)
            || (qbs.toolchainType === "clang" && cpp.compilerVersionMajor >= 16)) {
            return true;
        }
        console.info("Unsupported toolchainType " + qbs.toolchainType);
        return false;
    }
    consoleApplication: true
    files: [
        "ext.cppm",
        "ext.ixx",
        "ext.ccm",
        "ext.cxxm",
        "ext.c++m",
        "main.cpp",
    ]
    Group {
        fileTags: ["cppm"]
        files: ["ext.mpp"]
    }
    cpp.cxxLanguageVersion: "c++20"
    cpp.forceUseCxxModules: true
    cpp.treatWarningsAsErrors: true
}
