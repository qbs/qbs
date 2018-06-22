import qbs.File

Project {
    Application {
        name: "MyApp"
        files: ["myapp.blubb"]
        Depends { name: "blubber" }
    }
    StaticLibrary {
        name: "blubber"
        files: ["blubber.cpp"]
        Depends { name: "cpp" }
        Export {
            Depends { name: "cpp" }
            property bool enableTagger
            property string description: "Creating C++ source file.";
            FileTagger {
                condition: enableTagger
                patterns: ["*.blubb"]
                fileTags: ["blubb"]
            }
            Rule {
                inputs: ["blubb"]
                Artifact {
                    filePath: input.completeBaseName + ".cpp"
                    fileTags: ["cpp"]
                }
                prepare: {
                    var cmd = new JavaScriptCommand();
                    cmd.description = product.blubber.description;
                    cmd.sourceCode = function() {
                        File.copy(input.filePath, output.filePath);
                    }
                    return [cmd];
                }
            }
        }
    }
}
