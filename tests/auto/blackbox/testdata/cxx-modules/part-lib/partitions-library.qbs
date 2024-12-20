// Checks two modules split into partitions, one depends on the other
Project {
    DynamicLibrary {
        condition: {
            if (qbs.toolchainType === "msvc"
                || (qbs.toolchainType === "gcc" && cpp.compilerVersionMajor >= 11)
                || (qbs.toolchainType === "mingw" && cpp.compilerVersionMajor >= 13)
                || (qbs.toolchainType === "clang" && cpp.compilerVersionMajor >= 16)) {
                return true;
            }
            console.info("Unsupported toolchainType " + qbs.toolchainType);
            return false;
        }
        name: "liba"
        Depends { name: "cpp" }
        files: [
            "dllexport.h",
            "lib-a/a.cppm",
            "lib-a/a.p1.cppm",
            "lib-a/a.p2.cppm",
        ]
        install: true
        cpp.defines: "LIB_DYNAMIC"
        cpp.rpaths: cpp.rpathOrigin
        cpp.cxxLanguageVersion: "c++20"
        cpp.forceUseCxxModules: true
        cpp.treatWarningsAsErrors: true
        Depends { name: "bundle" }
        bundle.isBundle: false
    }
    DynamicLibrary {
        condition: {
            if (qbs.toolchainType === "msvc"
                || (qbs.toolchainType === "gcc" && cpp.compilerVersionMajor >= 11)
                || (qbs.toolchainType === "mingw" && cpp.compilerVersionMajor >= 13)
                || (qbs.toolchainType === "clang" && cpp.compilerVersionMajor >= 16))
                return true;
            console.info("Unsupported toolchainType " + qbs.toolchainType);
            return false;
        }
        name: "libb"
        Depends { name: "cpp" }
        Depends { name: "liba" }
        files: [
            "dllexport.h",
            "lib-b/b.cpp",
            "lib-b/b.cppm",
            "lib-b/b.p1.cppm",
            "lib-b/b.p2.cppm",
        ]
        install: true
        cpp.defines: "LIB_DYNAMIC"
        cpp.rpaths: cpp.rpathOrigin
        cpp.cxxLanguageVersion: "c++20"
        cpp.forceUseCxxModules: true
        cpp.treatWarningsAsErrors: true
        Depends { name: "bundle" }
        bundle.isBundle: false
    }
    CppApplication {
        name: "app"
        condition: {
            if (qbs.toolchainType === "msvc"
                || (qbs.toolchainType === "gcc" && cpp.compilerVersionMajor >= 11)
                || (qbs.toolchainType === "mingw" && cpp.compilerVersionMajor >= 13)
                || (qbs.toolchainType === "clang" && cpp.compilerVersionMajor >= 16))
                return true;
            console.info("Unsupported toolchainType " + qbs.toolchainType);
            return false;
        }
        Depends { name: "libb" }
        Depends { name: "liba" }
        consoleApplication: true
        files: [
            "app/main.cpp"
        ]
        install: true
        cpp.cxxLanguageVersion: "c++20"
        cpp.forceUseCxxModules: true
        cpp.treatWarningsAsErrors: true
    }
}
