import qbs
import qbs.FileInfo
import qbs.TextFile

Project {
    CppApplication {
        name: "compiler"
        files: ["compiler.cpp"]
    }
    Product {
        name: "p"
        type: ["mytype"]

        Depends { name: "compiler" }

        Rule {
            inputs: ["mytype.in"]
            explicitlyDependsOn: ["application"]
            Artifact {
                filePath: input.fileName + ".out"
                fileTags: product.type
            }
            prepare: {
                var compiler = explicitlyDependsOn["application"][0].filePath;
                var cmd = new Command(compiler, [input.filePath, output.filePath]);
                cmd.description = "compiling " + input.fileName;
                cmd.highlight = "compiler";
                return [cmd];
            }
        }

        Rule {
            multiplex: true
            explicitlyDependsOn: ["application"]
            Artifact {
                filePath: "compiler-name.txt"
                fileTags: product.type
            }
            prepare: {
                var nameCmd = new JavaScriptCommand();
                nameCmd.description = "writing compiler name";
                nameCmd.sourceCode = function() {
                    var compiler = explicitlyDependsOn["application"][0].filePath;
                    var file = new TextFile(output.filePath, TextFile.WriteOnly);
                    file.write("compiler file name: " + FileInfo.baseName(compiler));
                    file.close();
                }
                return [nameCmd];
            }
        }

        FileTagger {
            patterns: "*.in"
            fileTags: ["mytype.in"]
        }

        files: ["a.in", "b.in", "c.in"]
    }
}
