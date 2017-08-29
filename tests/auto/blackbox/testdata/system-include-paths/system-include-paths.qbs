import qbs

CppApplication {
    files: ["main.cpp"]
    cpp.systemIncludePaths: ["subdir"]
}
