DynamicLibrary {
    type: base.concat("custom")
    Depends { name: "cpp" }
    files: ["testlib.c"]
    Group {
        name: "version script"
        files: ["versionscript"]
        fileTags: ["versionscript"]
    }

    Rule {
        multiplex: true
        outputFileTags: "custom"
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.silent = true;
            cmd.sourceCode = function() {
                console.warn("---" + product.cpp.nmPath + "---");
            }
            return [cmd];
        }
    }

    qbs.installPrefix: ""
    install: true
    installDir: ""
}
