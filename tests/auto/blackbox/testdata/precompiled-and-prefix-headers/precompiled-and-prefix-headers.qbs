CppApplication {
    name: "MyApp"
    consoleApplication: true
    cpp.includePaths: [product.buildDirectory]
    cpp.prefixHeaders: [ "prefix.h" ]
    Group {
        files: ["pch.h"]
        fileTags: ["cpp_pch_src"]
    }
    files: ["main.cpp"]
}
