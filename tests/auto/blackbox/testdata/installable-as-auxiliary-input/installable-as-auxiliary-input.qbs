import qbs.File
import qbs.FileInfo
import qbs.TextFile

Project {
    name: "p"
    CppApplication {
        condition: {
            var result = qbs.targetPlatform === qbs.hostPlatform;
            if (!result)
                console.info("targetPlatform differs from hostPlatform");
            return result;
        }
        name: "app"
        Depends { name: "installed-header" }
        Rule {
            multiplex: true
            auxiliaryInputs: ["installable"]
            Artifact { filePath: "main.cpp"; fileTags: "cpp" }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = "creating " + output.fileName;
                cmd.sourceCode = function() {
                    var f = new TextFile(output.filePath, TextFile.WriteOnly);
                    f.writeLine("#include <theheader.h>");
                    f.writeLine("int main() {");
                    f.writeLine("    f();");
                    f.writeLine("}");
                    f.close();
                };
                return cmd;
            }
        }
    }

    Product {
        name: "installed-header"
        type: ["header"]

        property string installDir: "include"

        qbs.installPrefix: ""
        Group {
            fileTagsFilter: "header"
            qbs.install: true
            qbs.installDir: installDir
        }

        Export {
            Depends { name: "cpp" }
            cpp.includePaths: FileInfo.joinPaths(qbs.installRoot, product.installDir);
        }

        Rule {
            multiplex: true
            Artifact { filePath: "theheader.h.in"; fileTags: "header.in" }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = "Creating " + output.fileName;
                cmd.sourceCode = function() {
                    for (var i = 0; i < 1000; ++i) { // Artificial delay.
                        var file = new TextFile(output.filePath, TextFile.WriteOnly);
                        file.writeLine("#include <iostream>");
                        file.writeLine("inline void f() {");
                        file.writeLine('    std::cout << "f-impl" << std::endl;');
                        file.writeLine("}");
                        file.close();
                    }
                };
                return [cmd];
            }
        }
        Rule {
            inputs: "header.in"
            Artifact { filePath: "theheader.h"; fileTags: "header" }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = "Creating " + output.fileName;
                cmd.sourceCode = function() { File.copy(input.filePath, output.filePath); };
                return [cmd];
            }
        }
    }
}
