import qbs.FileInfo

BaseApp {
    inputNames: ["tool.1", "tool.2"]
    inputSearchPaths: "bin"
    outputFilePaths: ["bin/tool.2"]
    inputCandidateFilter: {
        return function(f) {
            return FileInfo.fileName(f) == "tool.2";
        }
    }
}
