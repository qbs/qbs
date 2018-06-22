Product {
    name: "archivable"
    type: "archiver.archive"
    Depends { name: "archiver" }
    archiver.workingDirectory: path
    Group {
        files: ["list.txt"]
        fileTags: ["archiver.input-list"]
    }
    files: ["test.txt"]
}
