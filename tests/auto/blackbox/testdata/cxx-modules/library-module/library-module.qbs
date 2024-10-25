// Checks module in a library
Project {
    DynamicLibrary {
        condition: {
            if (qbs.toolchainType === "msvc"
                || qbs.toolchainType === "gcc"
                // TODO: investigate why MinGW 11 fails
                || (qbs.toolchainType === "mingw" && cpp.compilerVersionMajor >= 13)
                || (qbs.toolchainType === "clang" && cpp.compilerVersionMajor >= 16))
                return true;
            console.info("Unsupported toolchainType " + qbs.toolchainType);
            return false;
        }
        name: "lib"
        Depends { name: "cpp" }
        files: [
            "lib/hello.cppm",
        ]
        install: true
        cpp.defines: "LIB_DYNAMIC"
        cpp.rpaths: cpp.rpathOrigin
        cpp.cxxLanguageVersion: "c++20"
        cpp.forceUseCxxModules: true
        cpp.treatWarningsAsErrors: true
    }
    CppApplication {
        condition: qbs.toolchainType === "msvc"
            || qbs.toolchainType === "gcc"
            || (qbs.toolchainType === "mingw" && cpp.compilerVersionMajor >= 13)
            || (qbs.toolchainType === "clang" && cpp.compilerVersionMajor >= 16)
        consoleApplication: true
        name: "app"
        Depends { name: "lib" }
        files: [
            "app/main.cpp"
        ]
        install: true
        cpp.rpaths: [cpp.rpathOrigin + "/../lib"]
        cpp.cxxLanguageVersion: "c++20"
        cpp.forceUseCxxModules: true
        cpp.treatWarningsAsErrors: true
    }
}