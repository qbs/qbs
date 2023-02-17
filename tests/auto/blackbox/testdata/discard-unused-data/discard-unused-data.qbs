CppApplication {
    name: "app"
    type: base.concat("custom")

    files: "main.cpp"

    Depends { name: "bundle"; condition: qbs.targetOS.includes("darwin") }
    Properties {
        condition: qbs.targetOS.includes("darwin")
        bundle.isBundle: false
    }

    Rule {
        multiplex: true
        outputFileTags: "custom"
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.silent = true;
            cmd.sourceCode = function() {
                console.info("is Darwin: " + product.qbs.targetOS.includes("darwin"));
                console.info("---" + product.cpp.nmPath + "---");
            }
            return [cmd];
        }
    }

}
