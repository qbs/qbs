import qbs

Module {
    Depends { name: "cpp" }

    Group {
        name: "Helper3 Sources"
        prefix: path + "/"
        files: ["helper3.c"]
    }

    validate: { throw "Nope."; }
}
