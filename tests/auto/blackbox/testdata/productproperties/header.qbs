import qbs.TextFile

Product {
    name: "blubb_header"
    type: "hpp"
    files: "blubb_header.h.in"
    property string blubbProp: project.blubbProp

    Rule {
        multiplex: true
        Artifact {
            filePath: "blubb_header.h"
            fileTags: "hpp"
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generating blubb_header.h";
            cmd.highlight = "codegen";
            cmd.blubbProp = product.blubbProp;
            cmd.sourceCode = function() {
                file = new TextFile(output.filePath, TextFile.WriteOnly);
                file.truncate();
                file.write("#define BLUBB_PROP " + blubbProp);
                file.close();
            }
            return cmd;
        }
    }

    Export {
        Depends { name: "cpp" }
        cpp.includePaths: product.buildDirectory
    }
}
