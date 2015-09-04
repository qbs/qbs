import qbs

Module {
    Depends { name: "cpp" }
    Depends { name: "helper5" }

    Group {
        name: "Helper4 Sources"
        prefix: path + "/"
        files: ["helper4.c"]
    }
}
