CppApplication {
    name: "app"
    files: ["main.cpp"]
    property string headerSubdir: "subdir1"
    cpp.includePaths: [path + "/" + headerSubdir]
}
