import qbs
import qbs.File

Project {
    Application {
        name: "MyApp"
        files: ["myapp.blubb"]
        Depends { name: "blubber" }
        Depends { name: "cpp" }
    }
    StaticLibrary {
        name: "blubber"
        files: ["blubber.cpp"]
        Depends { name: "cpp" }
        Export {
            FileTagger {
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
                    cmd.description = "Creating C++ source file.";
                    cmd.sourceCode = function() {
                        File.copy(input.filePath, output.filePath);
                    }
                    return [cmd];
                }
            }
        }
    }
}
