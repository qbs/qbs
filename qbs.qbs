import qbs 1.0

Project {
    property string version: "0.4.0"
    property bool enableUnitTests: false
    references: [
        "src/app/config/config.qbs",
        "src/app/config-ui/config-ui.qbs",
        "src/app/detect-toolchains/detect-toolchains.qbs",
        "src/app/qbs/qbs.qbs",
        "src/app/qbs-qmltypes/qbs-qmltypes.qbs",
        "src/app/qbs-setup-qt/qbs-setup-qt.qbs",
        "src/app/setupmaddeplatforms/setupmaddeplatforms.qbs",
        "src/lib/lib.qbs",
        "src/plugins/scanner/cpp/cpp.qbs",
        "src/plugins/scanner/qt/qt.qbs",
        "doc/doc.qbs",
        "tests/auto/blackbox/blackbox.qbs",
        "tests/auto/cmdlineparser/cmdlineparser.qbs",
    ].concat(unitTests)

    property list unitTests: enableUnitTests ? [
        "tests/auto/buildgraph/buildgraph.qbs",
        "tests/auto/language/language.qbs",
        "tests/auto/tools/tools.qbs"
    ] : []

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
                fileTags: "blubb"  // FIXME: Needs to be here because of qbs bug
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
