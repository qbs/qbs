// Checks two modules split into partitions, one depends on the other
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
        "a.p1.cppm",
        "a.p2.cppm",
        "b.cppm",
        "b.p1.cppm",
        "b.p2.cppm",
        "main.cpp"
    ]
    cpp.cxxLanguageVersion: "c++20"
    cpp.forceUseCxxModules: true
    cpp.treatWarningsAsErrors: true
}
