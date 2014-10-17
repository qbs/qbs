import qbs
import qbs.TextFile

Product {
    type: ["archiver.archive"]
    builtByDefault: false
    Depends { name: "archiver" }
    archiver.type: "tar"
    archiver.workingDirectory: qbs.installRoot

    Rule {
        multiplex: true
        inputsFromDependencies: ["installable"]
        Artifact {
            filePath: product.name + ".tarlist"
            fileTags: ["archiver.input-list"]
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.silent = true;
            cmd.sourceCode =function() {
                var ofile = new TextFile(output.filePath, TextFile.WriteOnly);
                for (var i = 0; i < inputs["installable"].length; ++i) {
                    var inp = inputs["installable"][i];
                    var installedFilePath = inp.moduleProperty("qbs", "installDir")
                            + '/' + inp.fileName;
                    while (installedFilePath[0] === '/')
                        installedFilePath = installedFilePath.substring(1);
                    ofile.writeLine(installedFilePath);
                }
                ofile.close();
            };
            return [cmd];
        }
    }
}
