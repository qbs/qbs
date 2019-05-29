BaseApp {
    inputSelectors: [
        "tool",
        ["tool.1", "tool.2"],
        {names : ["tool.3", "tool.4"]},
    ]
    inputSearchPaths: "bin"
    outputFilePaths: ["bin/tool", "bin/tool.1", "bin/tool.3"]
}
