import qbs.Environment
import qbs.File
import qbs.FileInfo
import qbs.Utilities

Product {
    name: "theProduct"
    type: ["mytype"]
    property string extension

    Rule {
        multiplex: true
        Artifact {
            filePath: "dummy"
            fileTags: product.type
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.silent = true;
            cmd.sourceCode = function() {
                var f = eval("new " + product.extension);
            };
            return [cmd];
        }
    }
}
