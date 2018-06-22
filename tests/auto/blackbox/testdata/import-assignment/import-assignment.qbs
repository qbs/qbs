import MyImport

Product {
    type: "outtype"
    property var importValue: MyImport
    Rule {
        multiplex: true
        Artifact {
            fileTags: "outtype"
            filePath: "dummy"
        }
        prepare: {
            var cmd = new JavaScriptCommand;
            cmd.silent = true;
            cmd.sourceCode = function() {
                console.info("key 1 = " + product.importValue.key1);
                console.info("key 2 = " + product.importValue.key2);
            };
            return cmd;
        }
    }
}
