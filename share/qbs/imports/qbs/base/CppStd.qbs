StaticLibrary {
    name: "CppStd"
    property bool useStdCompat: false
    Depends { name: "cpp" }
    cpp.cxxLanguageVersion: "c++23"
    cpp.forceUseCxxModules: true
    cpp.forceUseImportStd: true
    cpp.forceUseImportStdCompat: useStdCompat
    Properties {
        condition: qbs.toolchainType === "clang"
        cpp.cxxFlags: ["-Wno-reserved-module-identifier"]
        cpp.cxxStandardLibrary: "libc++"
    }
}
