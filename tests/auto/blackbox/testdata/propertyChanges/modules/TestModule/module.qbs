import qbs
import qbs.File

Module {
    FileTagger {
        pattern: "*.in"
        fileTags: "test-input"
    }

    Rule {
        inputs: ['test-input']
        Artifact {
            fileTags: "test-output"
            fileName: input.fileName + ".out"
        }

        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.highlight = "codegen";
            cmd.description = "Making output from input";
            cmd.sourceCode = function() {
            // print('Change in source code');
            File.copy(input.fileName, output.fileName);
            }
            return cmd;
        }
    }
}
