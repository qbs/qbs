BaseApp {
    inputNames: "tool"
    inputSearchPaths: "bin"
    inputNameFilter: {
        return function(n) {
            return n + ".2"
        };
    }
    outputFilePaths: ["bin/tool.2"]
    outputCandidatePaths: [["bin/tool.2"]]
}
