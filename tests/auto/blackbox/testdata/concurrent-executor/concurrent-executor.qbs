import qbs.File
import qbs.TextFile
import "util.js" as Utils

Product {
    type: ["final1", "final2"]
    Group {
        files: ["dummy1.input"]
        fileTags: ["input1"]
    }
    Group {
        files: ["dummy2.input"]
        fileTags: ["input2"]
    }
    Rule {
        inputs: ["input1"]
        Artifact {
            filePath: project.buildDirectory + "/dummy1.final"
            fileTags: ["final1"]
        }
        prepare: {
            var cmds = [];
            for (var i = 0; i < 10; ++i) {
                var cmd = new JavaScriptCommand();
                cmd.silent = true;
                cmd.createFile = i == 9;
                cmd.sourceCode = function() {
                    if (createFile) {
                        console.info("Creating file");
                        var file = new TextFile(output.filePath, TextFile.WriteOnly);
                        file.close();
                    }
                };
                cmds.push(cmd);
            }
            return cmds;
        }
    }
    Rule {
        inputs: ["input2"]
        Artifact {
            filePath: "dummy.intermediate"
            fileTags: ["intermediate"]
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.silent = true;
            cmd.sourceCode = function() { };
            return [cmd];
        }
    }
    Rule {
        inputs: ["intermediate"]
        outputFileTags: "final2"
        prepare: {
            do
                Utils.sleep(6000);
            while (!File.exists(project.buildDirectory + "/dummy1.final"));
            var cmd = new JavaScriptCommand();
            cmd.silent = true;
            cmd.sourceCode = function() { };
            return [cmd];
        }
    }
}


