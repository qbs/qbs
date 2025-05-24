StaticLibrary {
    condition: qbs.toolchain.includes("msvc")
        && !qbs.architecture.contains("arm64") && !qbs.architecture.contains("armv7")
    Depends  {  name: "cpp" }
    files: "msvc-asm-flags.asm"
    cpp.assemblerFlags: ["/I", "include"]
}
