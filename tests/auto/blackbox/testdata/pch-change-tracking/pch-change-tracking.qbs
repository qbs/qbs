import qbs

CppApplication {
    cpp.useCxxPrecompiledHeader: true
    files: [
        "header1.h",
        "header2.h",
        "main.cpp",
    ]
    Group {
        files: ["pch.h"]
        fileTags: ["cpp_pch_src"]
    }
}
