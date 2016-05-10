import qbs
import qbs.File

Product {
    type: "removal"
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
                File.remove(product.sourceDirectory + "/dir1");
            };
            return [cmd];
        }
    }
}
