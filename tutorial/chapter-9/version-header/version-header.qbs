//! [5]
// version-header/version-header.qbs
//! [0]
import qbs.TextFile

Product {
    name: "version_header"
    type: "hpp"

    Depends { name: "mybuildconfig" }
//! [0]

//! [1]
    Group {
        files: ["version.h.in"]
        fileTags: ["version_h_in"]
    }
//! [1]

//! [2]
    Rule {
        inputs: ["version_h_in"]
        Artifact {
            filePath: "version.h"
            fileTags: "hpp"
        }
//! [2]
//! [3]
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generating " + output.fileName;
            cmd.highlight = "codegen";
            cmd.sourceCode = function() {
                var file = new TextFile(input.filePath, TextFile.ReadOnly);
                var content = file.readAll();

                content = content.replace(
                    "${PRODUCT_VERSION}",
                    product.mybuildconfig.productVersion);

                file = new TextFile(output.filePath, TextFile.WriteOnly);
                file.write(content);
                file.close();
            }
            return cmd;
        }
//! [3]
    }

//! [4]
    Export {
        Depends { name: "cpp" }
        cpp.includePaths: exportingProduct.buildDirectory
    }
//! [4]
}
//! [5]
