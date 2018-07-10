import qbs.File
import qbs.TextFile

Project {
    Product {
        name: "dep"
        type: "dep_tag"

        files: "input.txt"

        FileTagger { patterns: "*.txt"; fileTags: "inp_tag" }

        Rule {
            inputs: "inp_tag"
            Artifact { filePath: input.baseName + ".intermediate"; fileTags: "int_tag" }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = "generating " + output.fileName;
                cmd.sourceCode = function() { File.copy(input.filePath, output.filePath); }
                return cmd;
            }
        }
        Rule {
            inputs: "int_tag"
            Artifact { filePath: input.baseName + ".dep"; fileTags: "dep_tag" }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = "generating " + output.fileName;
                cmd.sourceCode = function() { File.copy(input.filePath, output.filePath); }
                return cmd;
            }
        }
    }
    Product {
        name: "p"
        type: "p_tag"

        Depends { name: "dep" }

        Rule {
            inputsFromDependencies: "dep_tag"
            outputFileTags: "p_tag"
            outputArtifacts: {
                var dummy = new TextFile(input.filePath, TextFile.ReadOnly);
                dummy.close();
                return [{ filePath: input.baseName + ".p", fileTags: "p_tag" }]
            }

            prepare: {
                console.info("running final prepare script");
                var cmd = new JavaScriptCommand();
                cmd.description = "generating " + output.fileName;
                cmd.sourceCode = function() { File.copy(input.filePath, output.filePath); }
                return cmd;
            }
        }
    }
}
