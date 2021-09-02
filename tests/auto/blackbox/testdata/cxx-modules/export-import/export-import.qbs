// checks that module is re-exported. in this case, that module should be added to
// module map in dependent files
CppApplication {
    condition: {
        if (qbs.toolchainType === "msvc"
            || qbs.toolchainType === "gcc"
            || qbs.toolchainType === "mingw"
            || (qbs.toolchainType === "clang" && cpp.compilerVersionMajor >= 16))
            return true;
        console.info("Unsupported toolchainType " + qbs.toolchainType);
        return false;
    }
    consoleApplication: true
    files: [
        "a.cppm",
        "b.cppm",
        "c.cppm",
        "main.cpp"
    ]
    cpp.cxxLanguageVersion: "c++20"
    cpp.forceUseCxxModules: true
    cpp.treatWarningsAsErrors: true
}
