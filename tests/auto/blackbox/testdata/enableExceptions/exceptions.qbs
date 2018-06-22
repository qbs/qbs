CppApplication {
    files: ["main.cpp"]
    cpp.treatWarningsAsErrors: true
    cpp.defines: qbs.toolchain.contains("msvc") && !cpp.enableExceptions ? ["FORCE_FAIL_VS"] : []
}
