import qbs.TextFile

import qbs.Host

Project {
    Product {
        condition: {
            var result = qbs.targetPlatform === Host.platform() && qbs.architecture === Host.architecture();
            if (!result)
                console.info("target platform/arch differ from host platform/arch");
            return result;
        }
        name: 'someapp'
        type: 'application'
        consoleApplication: true

        property bool dummy: { console.info("object suffix: " + cpp.objectSuffix); }

        Depends { name: 'cpp' }
        Group {
            files: [ "main.cpp" ]
            fileTags: [ "foosource", "cpp" ]
        }
    }

    Rule {
        inputs: ["foosource"]
        Artifact {
            filePath: input.baseName + ".foo"
            fileTags: ["foo"]
        }

        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.sourceCode = "var file = new TextFile(output.filePath, TextFile.WriteOnly);";
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
            filePath: input.baseName + "_foo.cpp"
            fileTags: ["cpp"]
        }

        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.sourceCode = "var file = new TextFile(output.filePath, TextFile.WriteOnly);";
            cmd.sourceCode += "file.truncate();";
            cmd.sourceCode += "file.write(\"// There's nothing to see here!\\n\");";
            cmd.sourceCode += "file.write(\"int foo() { return 15; }\\n\");";
            cmd.sourceCode += "file.close();";
            cmd.description = "generating something";
            return cmd;
        }
    }
}

