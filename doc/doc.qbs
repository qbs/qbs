import qbs 1.0
import qbs.FileInfo
import qbs.Utilities

Product {
    name: "qbs documentation"
    builtByDefault: false
    type: "qch"
    Depends { name: "Qt.core" }
    Depends { name: "qbsbuildconfig" }
    Depends { name: "qbsversion" }

    files: [
        "external-resources.qdoc",
        "howtos.qdoc",
        "qbs.qdoc",
        "config/*.qdocconf",
        "reference/**/*",
    ]
    Group {
        name: "main qdocconf file"
        files: "qbs.qdocconf"
        fileTags: "qdocconf-main"
    }
    Group {
        name: "qtattribution.json files"
        files: [
            "../src/3rdparty/**/qt_attribution.json"
        ]
        fileTags: ["qtattribution"]
    }

    Rule {
        inputs: ["qtattribution"]
        Artifact {
            filePath: Utilities.getHash(input.filePath) + ".qtattributions.qdoc"
            fileTags: ["qdoc"]
        }

        prepare: {
            var tool = FileInfo.joinPaths(input.Qt.core.binPath, "qtattributionsscanner");
            var args = ["-o", output.filePath, FileInfo.joinPaths(FileInfo.path(input.filePath))];
            var cmd = new Command(tool, args);
            cmd.description = "running qtattributionsscanner";
            return cmd;
        }
    }

    property string versionTag: qbsversion.version.replace(/\.|-/g, "")
    Qt.core.qdocEnvironment: [
        "QBS_VERSION=" + qbsversion.version,
        "SRCDIR=" + path,
        "QT_INSTALL_DOCS=" + Qt.core.docPath,
        "QBS_VERSION_TAG=" + versionTag,
        "BUILDDIR=" + buildDirectory
    ]

    Group {
        fileTagsFilter: ["qdoc-output"]
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
