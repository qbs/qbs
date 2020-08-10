import qbs.FileInfo

BaseApp {
    inputNames: "tool"
    inputSearchPaths: ["bin", "usr/bin"]
     // env takes precedence
    inputEnvironmentPaths: "SEARCH_PATH"
    outputFilePaths: ["usr/bin/tool"]
    outputCandidatePaths: [["usr/bin/tool"]]
}
