Module {
    Depends { name: "cpp" }

    Group {
        name: "Helper3 Sources"
        files: ["helper3.c"]
    }

    validate: { throw "Nope."; }
}
