import qbs.TextFile

CppApplication {
    name: "theProduct"
    type: base.concat(["dummy"])
    Rule {
        multiplex: true
        Artifact {
            filePath: "generated.cpp"
            fileTags: ["dummy"]
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.silent = true;
            cmd.sourceCode = function() {
                var f = new TextFile(output.filePath, TextFile.WriteOnly);
                f.close();
            };
            return [cmd];
        }
    }

    Group {
        name: "the group"
        files: "**/*.cpp"
    }
}
