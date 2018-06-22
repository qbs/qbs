CppApplication {
    name: "app"
    type: base.concat("custom")

    files: "main.cpp"

    Depends { name: "bundle"; condition: qbs.targetOS.contains("darwin") }
    Properties {
        condition: qbs.targetOS.contains("darwin")
        bundle.isBundle: false
    }

    Rule {
        multiplex: true
        outputFileTags: "custom"
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.silent = true;
            cmd.sourceCode = function() {
                console.info("is Darwin: " + product.qbs.targetOS.contains("darwin"));
                console.info("---" + product.cpp.nmPath + "---");
            }
            return [cmd];
        }
    }

}
