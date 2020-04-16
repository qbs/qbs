import qbs
import qbs.Environment
import qbs.File
import qbs.FileInfo
import qbs.Probes
import qbs.TextFile
import qbs.Utilities
import qbs.Xml

Product {
    Depends { name: "qbsversion" }

    Probes.BinaryProbe {
        id: choco
        condition: qbs.targetOS.contains("windows")
        names: ["choco"]
        platformSearchPaths: {
            var chocolateyInstall = Environment.getEnv("ChocolateyInstall");
            if (chocolateyInstall)
                return [FileInfo.joinPaths(chocolateyInstall, "bin")];
            else
                return [FileInfo.joinPaths(Environment.getEnv("PROGRAMDATA"),
                                           "chocolatey", "bin")];
        }
    }

    condition: choco.found
    builtByDefault: false
    name: "qbs chocolatey"
    type: ["chocolatey.nupkg"]
    targetName: "qbs." + qbsversion.version
    destinationDirectory: project.buildDirectory

    property string chocoFilePath: choco.filePath

    Group {
        files: ["qbs.nuspec"]
        fileTags: ["chocolatey.nuspec"]
    }

    Group {
        files: ["chocolateyinstall.ps1"]
        fileTags: ["powershell.source"]
    }

    Group {
        files: ["../../../changelogs/*"]
        fileTags: ["changelog"]
    }

    Rule {
        inputs: ["chocolatey.nuspec", "powershell.source", "changelog"]
        multiplex: true

        Artifact {
            filePath: FileInfo.joinPaths(product.destinationDirectory,
                                         product.targetName + ".nupkg")
            fileTags: ["chocolatey.nupkg"]
        }

        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.silent = true;
            cmd.qbsVersion = product.qbsversion.version;
            cmd.powershellFilePath = inputs["powershell.source"][0].filePath;
            cmd.nuspecFileName = inputs["chocolatey.nuspec"][0].fileName;
            cmd.nuspecFilePath = inputs["chocolatey.nuspec"][0].filePath;
            cmd.chocoBuildDirectory = FileInfo.joinPaths(product.buildDirectory, "choco");
            cmd.chocoOutDirectory = FileInfo.path(outputs["chocolatey.nupkg"][0].filePath);
            cmd.changelogs = (inputs["changelog"] || []).map(function (a) {
                return {
                    filePath: a.filePath,
                    version: a.fileName.replace(/^changes-([0-9](\.[0-9]+)*)(\.md)?$/, "$1")
                };
            }).sort(function(a, b) {
                return Utilities.versionCompare(b.version, a.version);
            });
            cmd.sourceCode = function () {
                File.makePath(chocoBuildDirectory);
                File.makePath(FileInfo.joinPaths(chocoBuildDirectory, "tools"));

                var tf = new TextFile(FileInfo.joinPaths(
                                         chocoBuildDirectory, "tools", "chocolateyinstall.ps1"),
                                      TextFile.WriteOnly);
                try {
                    tf.writeLine("$qbsVersion = '" + qbsVersion + "'");
                    tf.writeLine("");
                    var tf2 = new TextFile(powershellFilePath, TextFile.ReadOnly);
                    try {
                        tf.write(tf2.readAll());
                    } finally {
                        tf2.close();
                    }
                } finally {
                    tf.close();
                }

                var doc = new Xml.DomDocument();
                doc.load(nuspecFilePath);
                var versionNode = doc.createElement("version");
                versionNode.appendChild(doc.createTextNode(qbsVersion));
                var releaseNotesNode = doc.createElement("releaseNotes");

                var releaseNotesText = "";
                changelogs.map(function (changelog) {
                    releaseNotesText += "qbs " + changelog.version + "\n\n";
                    var tf = new TextFile(changelog.filePath, TextFile.ReadOnly);
                    try {
                        releaseNotesText += tf.readAll() + "\n";
                    } finally {
                        tf.close();
                    }
                });
                releaseNotesNode.appendChild(doc.createTextNode(releaseNotesText.trim()));

                var metadataNode = doc.documentElement().firstChild("metadata");
                metadataNode.appendChild(versionNode);
                metadataNode.appendChild(releaseNotesNode);
                doc.save(FileInfo.joinPaths(chocoBuildDirectory, nuspecFileName));
            };
            var cmd2 = new Command(product.chocoFilePath,
                                   ["pack", FileInfo.joinPaths(cmd.chocoBuildDirectory,
                                                               cmd.nuspecFileName),
                                    "--limitoutput",
                                    "--outputdirectory", cmd.chocoOutDirectory]);
            cmd2.description = "choco pack " + inputs["chocolatey.nuspec"][0].fileName;
            return [cmd, cmd2];
        }
    }
}
