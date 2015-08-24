import qbs

Module {
    Depends { name: "cpp" }

    Group {
        name: "Helper6 Sources"
        prefix: path + "/"
        files: ["helper6.c"]
    }

    validate: { throw "Go away."; }
}
