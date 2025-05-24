import qbs.TextFile

Project {
    Product {
        name: "properties"
        type: ["properties"]
        Depends { name: "cpp" }
        Rule {
            multiplex: true
            Artifact {
                filePath: "properties.json"
                fileTags: ["properties"]
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.outputFilePath = outputs.properties[0].filePath;
                cmd.toolchain = product.qbs.toolchain;
                cmd.architecture = product.qbs.architecture;
                cmd.silent = true;
                cmd.sourceCode = function () {
                    var tf = new TextFile(outputFilePath, TextFile.WriteOnly);
                    try {
                        const data = {
                            "qbs.toolchain": toolchain,
                            "qbs.architecture": architecture,
                        };
                        tf.writeLine(JSON.stringify(data, undefined, 4));
                    } finally {
                        tf.close();
                    }
                };
                return [cmd];
            }
        }
    }
    StaticLibrary {
        name : "testa"
        files : [ "testa.s" ]
        Depends { name: "cpp" }
        condition: qbs.toolchain.includes("gcc")
        Properties {
            condition: qbs.targetOS.includes("darwin")
            bundle.isBundle: false
        }
    }
    StaticLibrary {
        name : "testb"
        files : [ "testb.S" ]
        Depends { name: "cpp" }
        condition: qbs.toolchain.includes("gcc")
        Properties {
            condition: qbs.targetOS.includes("darwin")
            bundle.isBundle: false
        }
    }
    StaticLibrary {
        name : "testc"
        files : [ "testc.sx" ]
        Depends { name: "cpp" }
        condition: qbs.toolchain.includes("gcc")
        Properties {
            condition: qbs.targetOS.includes("darwin")
            bundle.isBundle: false
        }
    }
    StaticLibrary {
        name: "testd"
        Group {
            condition: product.condition
            files: ["testd_" + qbs.architecture + ".asm"]
        }
        Depends { name: "cpp" }
        condition: qbs.toolchain.includes("msvc")
                   && (qbs.architecture === "x86" || qbs.architecture === "x86_64")
        Properties {
            condition: qbs.targetOS.includes("darwin")
            bundle.isBundle: false
        }
    }
}

