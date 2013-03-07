import qbs 1.0

Module {
    Depends {name : "cpp" }
    cpp.defines: ["HAVE_BLI"]
}

