Product {
    Group {
        name: "recursive"
        files: "**/file.txt"
    }
    Group {
        name: "directories"
        prefix: "nonrecursive/"
        files: "subdi?/file.txt"
    }
    Group {
        prefix: "nonrecursive/empty/"
        name: "no files"
        files: "*.txt"
    }
}
