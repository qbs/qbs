import qbs.FileInfo

BaseApp {
    inputNames: ["tool.1", "tool.2"]
    inputSearchPaths: "bin"
    inputCandidateFilter: {
        return function(f) {
            return FileInfo.fileName(f) == "tool.2";
        }
    }
    outputFilePaths: ["bin/tool.2"]
    outputCandidatePaths: [["bin/tool.1", "bin/tool.2"]]
}
