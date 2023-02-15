CppApplication {
    files: ["main.cpp"]
    cpp.treatWarningsAsErrors: true
    cpp.defines: qbs.toolchain.includes("msvc") && !cpp.enableExceptions ? ["FORCE_FAIL_VS"] : []
}
