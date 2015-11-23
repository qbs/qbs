import qbs

Module {
    Depends { name: "cpp" }

    Group {
        name: "Helper2 Sources"
        prefix: path + "/"
        files: ["helper2.c"]
    }

    Group {
        name: "some other file from helper2"
        prefix: product.sourceDirectory + '/'
        files: ["someotherfile.txt"]
    }
}
