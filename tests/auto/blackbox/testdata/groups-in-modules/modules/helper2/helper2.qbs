import qbs

Module {
    Depends { name: "cpp" }

    Group {
        name: "Helper2 Sources"
        prefix: path + "/"
        files: ["helper2.c"]
    }
}
