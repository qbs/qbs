import qbs.File
import qbs.TextFile

MyApplication {
    name: "myapp"
    type: base.concat("extra-output2")
    property bool dummy: { console.info("executable suffix: " + cpp.executableSuffix); }
    files: "main.cpp"
    Group {
        fileTagsFilter: "application"
        qbs.installDir: "binDir"
        fileTags: "extra-input"
    }
    Group {
        fileTagsFilter: "extra-output"
        qbs.installPrefix: "fromFilterGroup"
    }
    Rule {
        inputs: "extra-input"
        Artifact {
            filePath: input.baseName + ".txt"
            fileTags: "extra-output"
            qbs.installDir: "fromArtifact"
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "creating " + output.fileName;
            cmd.sourceCode = function() {
                var f = new TextFile(output.filePath, TextFile.WriteOnly);
                f.close();
            }
            return cmd;
        }
    }
    Rule {
        inputs: "extra-output"
        Artifact {
            filePath: "dummy"
            fileTags: "extra-output2"
        }
        prepare: {
            var cmd = new JavaScriptCommand;
            cmd.description = "checker";
            cmd.sourceCode = function() {
                console.info("installPrefix: " + input.qbs.installPrefix);
                console.info("installDir: " + input.qbs.installDir);
                File.copy(input.filePath, output.filePath);
            };
            return cmd;
        }
    }
}
