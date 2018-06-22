import qbs.Probes

Module {
    Probes.PkgConfigProbe {
        id: theProbe
        name: "dummy"
    }

    property stringList libs: theProbe.libs

    Rule {
        multiplex: true
        outputFileTags: "theType"
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.silent = true;
            cmd.sourceCode = function() {
                console.info(product.name + " libs: " + JSON.stringify(product.themodule.libs));
            }
            return [cmd];
        }
    }
}
