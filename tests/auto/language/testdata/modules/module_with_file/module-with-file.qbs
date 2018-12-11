Module {
    property bool file1IsTarget
    property bool file2IsTarget
    Group {
        prefix: product.sourceDirectory + '/'
        files: "zort"
        filesAreTargets: product.module_with_file.file1IsTarget
    }
    Group {
        prefix: product.sourceDirectory + '/'
        files: "zort"
        filesAreTargets: product.module_with_file.file2IsTarget
    }
}
