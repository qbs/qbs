Module {
    Depends { name: "cpp" }

    Group {
        name: "Helper6 Sources"
        files: ["helper6.c"]
    }

    validate: { throw "Go away."; }
}
