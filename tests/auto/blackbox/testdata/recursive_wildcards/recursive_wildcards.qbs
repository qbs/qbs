Product {
    type: "installed_content"
    Group {
        fileTags: "install"
        files: "dir/*"
        recursive: true
        qbs.installDir: "dir"
    }
}
