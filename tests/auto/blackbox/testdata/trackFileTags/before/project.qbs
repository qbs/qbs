import qbs 1.0
import qbs.TextFile

Project {
    Product {
        name: 'someapp'
        type: 'application'
        Depends { name: 'cpp' }
        Group {
            files: [ "main.cpp" ]
            fileTags: [ "cpp" ]
        }
    }

    Rule {
        inputs: ["foosource"]
        Artifact {
            fileName: input.baseName + ".foo"
            fileTags: ["foo"]
        }

        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.sourceCode = "var file = new TextFile(output.fileName, TextFile.WriteOnly);";
            cmd.sourceCode += "file.truncate();"
            cmd.sourceCode += "file.write(\"There's nothing to see here!\");"
            cmd.sourceCode += "file.close();"
            cmd.description = "generating something";
            return cmd;
        }
    }

    Rule {
        inputs: ["foo"]
        Artifact {
            fileName: input.baseName + "_foo.cpp"
            fileTags: ["cpp"]
        }

        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.sourceCode = "var file = new TextFile(output.fileName, TextFile.WriteOnly);";
            cmd.sourceCode += "file.truncate();";
            cmd.sourceCode += "file.write(\"// There's nothing to see here!\\n\");";
            cmd.sourceCode += "file.write(\"int foo() { return 15; }\\n\");";
            cmd.sourceCode += "file.close();";
            cmd.description = "generating something";
            return cmd;
        }
    }
}

