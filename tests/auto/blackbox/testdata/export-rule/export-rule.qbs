import qbs
import qbs.File

Project {
    Application {
        name: "MyApp"
        Group {
            files: ["myapp.blubb"]
            fileTags: ["blubb"]
        }
        Depends { name: "blubber" }
        Depends { name: "cpp" }
    }
    StaticLibrary {
        name: "blubber"
        files: ["blubber.cpp"]
        Depends { name: "cpp" }
        Export {
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
