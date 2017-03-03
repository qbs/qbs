import qbs

Product {
    type: "custom"
    Depends { name: "mymodule" }
    Rule {
        multiplex: true
        Artifact {
            filePath: "dummy.custom"
            fileTags: ["custom"]
        }
        prepare: {
            var theProperty = product.mymodule.theProperty;
            if (!theProperty)
                throw "Oh no!";
            var dummy = new JavaScriptCommand();
            dummy.silent = true;
            dummy.sourceCode = function() {};
            return [dummy];
        }
    }
}
