import qbs.FileInfo

Module {
    Scanner {
        inputs: "i"
        searchPaths: [FileInfo.path(input.filePath)]
        scan: ["file.inc"]
    }
}
