import qbs

CppApplication {
    files: ["main.m"]
    fileTags: ["objcpp"]
    cpp.frameworks: ["Foundation"]
}
