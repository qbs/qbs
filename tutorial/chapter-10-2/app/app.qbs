CppApplication {
    condition: qbs.toolchainType === "msvc"
        || (qbs.toolchainType === "gcc" && cpp.compilerVersionMajor >= 11)
        || (qbs.toolchainType === "mingw" && cpp.compilerVersionMajor >= 13)
        || (qbs.toolchainType === "clang" && cpp.compilerVersionMajor >= 16)
    consoleApplication: true
    name: "app"

    files: ["main.cpp"]
    install: true

    cpp.rpaths: [cpp.rpathOrigin + "/../lib"]
    cpp.cxxLanguageVersion: "c++20"
    cpp.forceUseCxxModules: true
    cpp.treatWarningsAsErrors: true

    Depends { name: "mylib" }
}
