import qbs 1.0

Project {
    property string version: "1.0.1"
    property bool enableUnitTests: false

    references: [
        "src/src.qbs",
        "doc/doc.qbs",
        "tests/auto/auto.qbs"
    ]

    Product {
        name: "share"
        Group {
            files: ["share/qbs"]
            qbs.install: true
            qbs.installDir: "share"
        }

        Transformer {
            inputs: "share/qbs"
            Artifact {
                fileName: "share/qbs"
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = "Copying share/qbs to build directory.";
                cmd.highlight = "codegen";
                cmd.sourceCode = function() { File.copy(input.fileName, output.fileName); }
                return cmd;
            }
        }
    }
}
