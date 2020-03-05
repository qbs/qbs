import qbs.FileInfo
import qbs.TextFile

CppApplication {
    name: "app"
    cpp.includePaths: buildDirectory
    Group {
        files: "main.cpp"
        fileTags: ["cpp", "custom.in"]
    }
    Rule {
        inputs: "custom.in"
        Artifact {
            filePath: FileInfo.completeBaseName(input.filePath) + ".h"
            fileTags: "hpp"
        }
        Artifact {
            filePath: "custom.txt"
            fileTags: "whatever"
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generating " + outputs.hpp[0].fileName;
            cmd.sourceCode = function() {
                var f = new TextFile(outputs.hpp[0].filePath, TextFile.WriteOnly);
                f.writeLine("int main() {}");
                f.close();
                f = new TextFile(outputs.whatever[0].filePath, TextFile.WriteOnly);
                f.close();
            }
            return cmd;
        }
    }
}
