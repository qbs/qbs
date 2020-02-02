BaseApp {
    inputSelectors: [
        "tool",
        ["tool.0", "tool.1", "tool.2"],
        {names : ["tool.3", "tool.4"]},
    ]
    inputSearchPaths: "bin"
    outputFilePaths: ["bin/tool", "bin/tool.1", "bin/tool.3"]
    outputCandidatePaths: [["bin/tool"], ["bin/tool.0", "bin/tool.1"], ["bin/tool.3"]]
}
