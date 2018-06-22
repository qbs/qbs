import qbs.FileInfo
import qbs.TextFile

Module {
    name: "script-test"

    Rule {
        inputs: ["script"]

        Artifact {
            filePath: FileInfo.joinPaths(project.buildDirectory, input.fileName)
            fileTags: ["application"]
        }

        prepare: {
            var cmds = [];
            var cmd = new JavaScriptCommand();
            cmd.description = "copying " + input.fileName;
            cmd.sourceCode = function() {
                var tf = new TextFile(input.filePath, TextFile.ReadOnly);
                var s = tf.readAll().replace("$PWD", project.buildDirectory);
                tf.close();
                var tf2 = new TextFile(output.filePath, TextFile.ReadWrite);
                tf2.write(s);
                tf2.close();
            };
            cmds.push(cmd);

            if (output.fileName !== "script-noexec") {
                var cmd2 = new Command("chmod", ["+x", output.filePath]);
                cmd2.silent = true;
                cmds.push(cmd2);
            }

            return cmds;
        }
    }
}
