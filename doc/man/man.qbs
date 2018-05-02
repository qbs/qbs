import qbs
import qbs.File
import qbs.FileInfo
import qbs.ModUtils
import qbs.Probes
import qbs.Utilities

Product {
    name: "qbs man page"
    type: ["manpage"]

    Depends { productTypes: ["qbsapplication"]; condition: updateManPage }
    Depends { name: "qbsbuildconfig" }

    property bool updateManPage: false
    property string help2ManFilePath: help2ManProbe.filePath

    Group {
        name: "man page"
        files: ["qbs.1"]
        qbs.install: qbsbuildconfig.installManPage
        qbs.installDir: "share/man/man1"
    }

    Group {
        name: "additional sections"
        files: ["see-also.h2m"]
        fileTags: ["man.section"]
    }

    Rule {
        condition: updateManPage
        multiplex: true
        // TODO: Remove in 1.14.
        explicitlyDependsOn: ["application"]
        property stringList explicitlyDependsOnFromDependencies: ["application"]
        inputs: ["man.section"]
        Artifact {
            filePath: "qbs.1"
            fileTags: ["manpage"]
        }
        prepare: {
            var help2ManExe = product.help2ManFilePath;
            if (!help2ManExe)
                throw "Cannot update man page: help2man not available";
            if (Utilities.versionCompare(product.qbs.version, "1.9") < 0)
                throw "Cannot update man page: qbs >= 1.9 required";
            var qbsApp;
            for (var i = 0; i < explicitlyDependsOn.application.length; ++i) {
                var artifact = explicitlyDependsOn.application[i];
                if (artifact.product.name === "qbs_app")
                    qbsApp = ModUtils.artifactInstalledFilePath(artifact);
            }
            var args = [qbsApp, "-o", output.filePath,
                        "--no-info", "--name=the Qbs build tool"];
            var sections = inputs ? inputs["man.section"] : [];
            for (var i = 0; i < sections.length; ++i)
                args.push("--include=" + sections[i].filePath);
            var help2ManCmd = new Command(help2ManExe, args);
            help2ManCmd.description = "creating man page";
            var copyCmd = new JavaScriptCommand();
            copyCmd.silent = true;
            copyCmd.sourceCode = function() {
                File.copy(output.filePath,
                          FileInfo.joinPaths(product.sourceDirectory, output.fileName));
            }
            return [help2ManCmd, copyCmd];
        }
    }

    Probes.BinaryProbe {
        id: help2ManProbe
        names: ["help2man"]
    }
}
