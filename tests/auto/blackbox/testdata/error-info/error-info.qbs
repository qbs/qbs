import "helper.js" as Helper

Project {
    property bool fail1: false
    property bool fail2: false
    property bool fail3: false
    property bool fail4: false
    property bool fail5: false
    property bool fail6: false
    property bool fail7: false

    Product {
        name: "myproduct"
        type: ["foo", "bar"]

        Rule {
            multiplex: true

            Artifact {
                fileTags: ["foo"]
                filePath: {
                    var path = "foo";
                    if (project.fail1)
                        throw "fail1";
                    return path;
                }
            }

            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.sourceCode = function () {

                };
                cmd.silent = true;
                if (project.fail2)
                    generate.an.error;
                if (project.fail6)
                    Helper.doSomethingEvil();
                return cmd;
            }
        }

        Rule {
            multiplex: true

            outputFileTags: ["bar"]
            outputArtifacts: {
                var list = [];
                list.push({ fileTags: ["bar"], filePath: "bar" });
                if (project.fail3)
                    throw "fail3";
                if (project.fail5)
                    Helper.doSomethingBad();
                return list;
            }

            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.fail7 = project.fail7;
                cmd.sourceCode = function () {
                    if (fail7)
                        will.fail;
                };
                cmd.silent = true;
                if (project.fail4)
                    generate.an.error;
                return cmd;
            }
        }
    }
}
