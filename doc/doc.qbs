import qbs 1.0
import qbs.File
import qbs.FileInfo
import qbs.Probes

Project {
    references: ["man/man.qbs"]

    Product {
        name: "qbs documentation"
        builtByDefault: false
        type: Qt.core.config.contains("cross_compile") ?
                  ["qbsdoc.qdoc-html-fixed"] : [ "qbsdoc.qdoc-html-fixed", "qch"]

        property string fixedHtmlDir: FileInfo.joinPaths(buildDirectory, "qdoc-html-fixed")
        Depends { name: "Qt.core" }
        Depends { name: "qbsbuildconfig" }
        Depends { name: "qbsversion" }

        Probes.BinaryProbe {
            id: pythonProbe
            names: ["python3", "python"] // on Windows, there's no python3
        }
        property string _pythonExe: pythonProbe.found ? pythonProbe.filePath : undefined

        files: [
            "../README.md",
            "../CONTRIBUTING.md",
            "classic.css",
            "external-resources.qdoc",
            "fixnavi.pl",
            "howtos.qdoc",
            "qbs.qdoc",
            "qbs-online.qdocconf",
            "config/*.qdocconf",
            "config/style/qt5-sidebar.html",
            "appendix/**/*",
            "reference/**/*",
            "templates/**/*",
            "images/**",
            "targets/**",
        ]
        Group {
            name: "main qdocconf file"
            files: "qbs.qdocconf"
            fileTags: "qdocconf-main"
        }
        Group {
            name: "fix-imports script"
            files: ["fix-qmlimports.py"]
            fileTags: ["qbsdoc.fiximports"]
        }

        property string versionTag: qbsversion.version.replace(/\.|-/g, "")
        Qt.core.qdocEnvironment: [
            "QBS_VERSION=" + qbsversion.version,
            "SRCDIR=" + path,
            "QT_INSTALL_DOCS=" + Qt.core.docPath,
            "QBS_VERSION_TAG=" + versionTag
        ]

        Rule {
            inputs: ["qdoc-png"]
            explicitlyDependsOn: ["qbsdoc.fiximports"]
            multiplex: true
            outputFileTags: ["qdoc-html", "qbsdoc.dummy"] // TODO: Hack. Rule injection to the rescue?
            outputArtifacts: [{filePath: "dummy", fileTags: ["qbsdoc.dummy"]}]
            prepare: {
                if (!product._pythonExe)
                    throw "Python executable was not found";
                var scriptPath = explicitlyDependsOn["qbsdoc.fiximports"][0].filePath;
                var htmlDir = FileInfo.path(FileInfo.path(inputs["qdoc-png"][0].filePath));
                var fixCmd = new Command(product._pythonExe, [scriptPath, htmlDir]);
                fixCmd.description = "fixing bogus QML import statements";
                return [fixCmd];
            }
        }

        Rule {
            inputs: ["qdoc-html"]
            Artifact {
                filePath: FileInfo.joinPaths(product.fixedHtmlDir, input.fileName)
                fileTags: ["qbsdoc.qdoc-html-fixed"]
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.silent = true;
                cmd.sourceCode = function() { File.copy(input.filePath, output.filePath); }
                return [cmd];
            }
        }

        Group {
            fileTagsFilter: ["qbsdoc.qdoc-html-fixed"]
            qbs.install: qbsbuildconfig.installHtml
            qbs.installDir: qbsbuildconfig.docInstallDir
            qbs.installSourceBase: product.fixedHtmlDir
        }
        Group {
            fileTagsFilter: ["qdoc-css", "qdoc-png"]
            qbs.install: qbsbuildconfig.installHtml
            qbs.installDir: qbsbuildconfig.docInstallDir
            qbs.installSourceBase: Qt.core.qdocOutputDir
        }
        Group {
            fileTagsFilter: ["qch"]
            qbs.install: qbsbuildconfig.installQch
            qbs.installDir: qbsbuildconfig.docInstallDir
        }
    }
}
