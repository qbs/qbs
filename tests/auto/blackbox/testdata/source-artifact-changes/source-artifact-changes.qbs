CppApplication {
    name: "app"
    type: base.concat("dummy")
    consoleApplication: true

    Properties {
        condition: qbs.targetOS.contains("darwin")
        bundle.embedInfoPlist: false
    }

    Probe {
        id: toolchainProbe
        property stringList toolchain: qbs.toolchain
        configure: {
            console.info("is gcc: " + toolchain.contains("gcc"));
            found = true;
        }
    }

    Rule {
        multiplex: true
        inputs: "cpp"
        Artifact {
            filePath: "dummy"
            fileTags: "dummy"
            cpp.cxxLanguageVersion: "hoppla"
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.silent = true;
            cmd.sourceCode = function() {
                if (output.cpp.cxxLanguageVersion !== "hoppla")
                    throw "This cannot be!";
            };
            return cmd;
        }
    }

    Rule {
        multiplex: true
        inputs: "cpp"
        requiresInputs: false
        Artifact { filePath: "dummy2"; fileTags: "dummy" }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.silent = true;
            cmd.sourceCode = function() {
                console.info("cpp artifacts: "
                             + (product.artifacts.cpp ? product.artifacts.cpp.length : 0))
            };
            return cmd;
        }
    }

    Depends { name: "module_with_files" }
}
