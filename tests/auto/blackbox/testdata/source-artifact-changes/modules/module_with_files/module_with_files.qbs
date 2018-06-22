Module {
    property stringList fileTags
    property bool overrideTags
    property bool filesAreTargets

    Depends { name: "cpp" }

    Group {
        prefix: path + '/'
        files: "main.cpp"
        fileTags: product.module_with_files.fileTags
        overrideTags: product.module_with_files.overrideTags
        filesAreTargets: product.module_with_files.filesAreTargets
    }
}
