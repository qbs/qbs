import qbs.File

Module {
    FileTagger {
        patterns: ["*.in"]
        fileTags: "test-input"
    }

    property string testProperty

    Rule {
        inputs: ['test-input']
        Artifact {
            fileTags: "test-output"
            filePath: input.fileName + ".out"
        }

        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.highlight = "codegen";
            cmd.description = "Making output from input";
            cmd.sourceCode = function() {
                // console.info('Change in source code');
                console.info(input.TestModule.testProperty);
                File.copy(input.filePath, output.filePath);
            }
            var dummyCmd = new JavaScriptCommand();
            dummyCmd.silent = true;
            dummyCmd.sourceCode = function() {};
            return [cmd, dummyCmd];
        }
    }
}
