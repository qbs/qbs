import qbs.File
import qbs.TextFile

Project {
    Product {
        name: "dep"
        type: "the_tag"
        Rule {
            multiplex: true
            Artifact {
                filePath: "file1.txt"
                fileTags: "the_tag"
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = "creating " + output.fileName;
                cmd.sourceCode = function() {
                    var f = new TextFile(output.filePath, TextFile.WriteOnly);
                    f.writeLine("the_content");
                    f.close();
                };
                return cmd;
            }
        }
        Rule {
            inputs: "the_tag"
            excludedInputs: "the_other_tag"
            Artifact {
                filePath: "file2.txt"
                fileTags: "the_other_tag"
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = "creating " + output.fileName;
                cmd.sourceCode = function() {
                    File.copy(input.filePath, output.filePath);
                    var f = new TextFile(output.filePath, TextFile.Append);
                    f.writeLine("the_other_content");
                    f.close();
                };
                return cmd;
            }
        }
        Group {
            fileTagsFilter: "the_other_tag"
            fileTags: "the_tag"
        }
    }
    Product {
        name: "p"
        type: "p_type"
        Depends { name: "dep" }
        Rule {
            multiplex: true
            inputsFromDependencies: "the_tag"
            Artifact {
                filePath: "dummy1.txt"
                fileTags: "p_type"
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = "creating " + output.fileName;;
                if (!inputs["the_tag"] || inputs["the_tag"].length != 2)
                    throw "Huch?";
                cmd.sourceCode = function() {
                    var f = new TextFile(output.filePath, TextFile.WriteOnly);
                    f.close();
                };
                return cmd;
            }
        }
        Rule {
            multiplex: true
            inputsFromDependencies: "the_tag"
            excludedInputs: "the_other_tag"
            Artifact {
                filePath: "dummy2.txt"
                fileTags: "p_type"
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = "creating " + output.fileName;;
                if (!inputs["the_tag"] || inputs["the_tag"].length != 1)
                    throw "Huch?";
                cmd.sourceCode = function() {
                    var f = new TextFile(output.filePath, TextFile.WriteOnly);
                    f.close();
                };
                return cmd;
            }
        }
        Rule {
            multiplex: true
            explicitlyDependsOnFromDependencies: "the_tag"
            excludedInputs: "the_other_tag"
            Artifact {
                filePath: "dummy3.txt"
                fileTags: "p_type"
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = "creating " + output.fileName;
                if (!explicitlyDependsOn["the_tag"] || explicitlyDependsOn["the_tag"].length != 1)
                    throw "Huch?";
                cmd.sourceCode = function() {
                    var f = new TextFile(output.filePath, TextFile.WriteOnly);
                    f.close();
                };
                return cmd;
            }
        }
    }
}
