import qbs.File

Product {
    type: "removal"
    Rule {
        multiplex: true
        outputFileTags: "removal"
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
