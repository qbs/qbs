CppApplication {
    name: "theApp"
    property bool enableTagger
    files: ["main.custom"];
    FileTagger {
        condition: enableTagger
        patterns: ["*.custom"]
        fileTags: ["cpp"]
    }
}
