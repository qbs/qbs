BaseApp {
    inputSelectors: [
        "tool.1",
        ["tool.2"],
        {names : "tool.3"},
        {names : ["tool.4"]}
    ]
    inputSearchPaths: "bin"
    outputFilePaths: ["bin/tool.1", "bin/tool.2", "bin/tool.3", "bin/tool.4"]
    outputCandidatePaths: [["bin/tool.1"], ["bin/tool.2"], ["bin/tool.3"], ["bin/tool.4"]]
}
