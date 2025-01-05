Product {
    type: "custom"
    Depends { name: "mymodule" }
    qbsModuleProviders: "provider"
    Rule {
        multiplex: true
        outputFileTags: ["custom"]
        prepare: {
            var theProperty = product.mymodule.listProp;
            if (!theProperty)
                throw "Oh no!";
            var dummy = new JavaScriptCommand();
            dummy.silent = true;
            dummy.sourceCode = function() {};
            return [dummy];
        }
    }
}
