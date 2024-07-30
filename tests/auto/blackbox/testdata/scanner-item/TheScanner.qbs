import qbs.FileInfo

Scanner {
    inputs: "i"
    searchPaths: [FileInfo.path(input.filePath)]
    scan: ["file.inc"]
}
