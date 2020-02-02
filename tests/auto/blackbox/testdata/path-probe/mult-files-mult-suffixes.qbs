BaseApp {
    inputSelectors: [
        {names : "tool", nameSuffixes: [".0", ".1", ".2"]},
        {names : "super-tool", nameSuffixes: [".1"]},
    ]
    inputSearchPaths: "bin"
    outputFilePaths: ["bin/tool.1", "bin/super-tool.1"]
    outputCandidatePaths: [["bin/tool.0", "bin/tool.1"], ["bin/super-tool.1"]]
}
