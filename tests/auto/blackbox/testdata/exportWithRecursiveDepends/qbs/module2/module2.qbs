import qbs

Module {
    Depends { name: "cpp" }
    cpp.defines: ["HAS_FOO"]
}
