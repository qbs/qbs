import qbs.File

Project {
    Product {
        name: "dep"
        type: "dep.out"
        Group {
            files: "file.in"
            fileTags: "dep.in"
        }
        Rule {
            inputs: "dep.in"
            outputFileTags: "dep.out"
            outputArtifacts: {
                if (!product.artifacts["dep.in"] || product.artifacts["dep.in"].length !== 1)
                    throw "source file not in artifacts map!"
                if (product.artifacts["dep.out"] && product.artifacts["dep.out"].length !== 0)
                    throw "generated artifact already in map!";
                return [{filePath: "file.out", fileTags: "dep.out"}];
            }
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
        type: "p.out"
        Depends { name: "dep" }
        Rule {
            inputsFromDependencies: "dep.out"
            Artifact { filePath: "myfile.out"; fileTags: "p.out" }
            prepare: {
                var dep;
                for (var i = 0; i < product.dependencies.length; ++i) {
                    if (product.dependencies[i].name === "dep") {
                        dep = product.dependencies[i];
                        break;
                    }
                }
                if (!dep)
                    throw "dependency not found";
                if (!dep.artifacts["dep.in"] || dep.artifacts["dep.in"].length !== 1)
                    throw "source file not in dependency's artifacts map!"
                if (!dep.artifacts["dep.out"] || dep.artifacts["dep.out"].length !== 1)
                    throw "generated artifact not in dependency's artifacts map!";
                var cmd = new JavaScriptCommand();
                cmd.description = "generating " + output.fileName;
                cmd.sourceCode = function() { File.copy(input.filePath, output.filePath); }
                return cmd;
            }
        }
    }
}
