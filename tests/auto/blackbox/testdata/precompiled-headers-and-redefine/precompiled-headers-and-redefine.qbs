CppApplication {
    name: "MyApp"
    consoleApplication: true
    Group {
        files: ["pch.h"]
        fileTags: ["cpp_pch_src"]
    }
    Group {
        files: ["file.cpp"]
        cpp.defines: ["MYDEF=1"]
        cpp.cxxFlags: base.concat(qbs.toolchain.contains("clang-cl") ? ["-Wno-clang-cl-pch"] : [])
    }
    cpp.treatWarningsAsErrors: true

    files: ["main.cpp"]
}
