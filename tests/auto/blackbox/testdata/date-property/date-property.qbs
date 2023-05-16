Product {
    type: "date"
    property var theDate: new Date(1999, 11, 31);
    Rule {
        multiplex: true
        Artifact { filePath: "dummy"; fileTags: "date" }
        prepare: {
            var cmd = new JavaScriptCommand;
            cmd.silent = true;
            cmd.sourceCode = function() {
                var d = product.theDate;
                console.info("The stored date was " + d.getFullYear() + '-' + (d.getMonth() + 1) + '-'
                             + d.getDate());
            };
            return cmd;
        }
    }
}
