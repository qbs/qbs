StaticLibrary {
    condition: qbs.toolchain.includes("msvc")
    Depends  {  name: "cpp" }
    files: "msvc-asm-flags.asm"
    cpp.assemblerFlags: ["/I", "include"]
}
