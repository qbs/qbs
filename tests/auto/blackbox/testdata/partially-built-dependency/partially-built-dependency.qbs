import qbs.File
import qbs.TextFile

Project {
    Product {
        name: "p"
        type: "obj"
        Depends { name: "cpp" }
        Depends { name: "dep" }
        Rule {
            inputsFromDependencies: "cpp"
            Artifact {
                filePath: input.fileName
                fileTags: "cpp"
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description ="copying " + input.fileName;
                cmd.sourceCode = function() { File.copy(input.filePath, output.filePath); }
                return cmd;
            }
        }
    }
    CppApplication {
        name: "dep"
        Rule {
            multiplex: true
            Artifact { filePath: "main.cpp"; fileTags: "cpp" }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = "generating " + output.fileName;
                cmd.sourceCode = function() {
                    var f = new TextFile(output.filePath, TextFile.WriteOnly);
                    f.writeLine("int main() {}");
                }
                return cmd;
            }
        }
    }
}
