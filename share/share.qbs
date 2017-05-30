import qbs
import qbs.File
import qbs.FileInfo

Product {
    name: "qbs resources"
    type: ["copied qbs resources"]
    Depends { name: "qbsbuildconfig" }

    Group {
        name: "Incredibuild"
        prefix: "../bin/"
        files: ["ibmsvc.xml", "ibqbs.bat"]
        fileTags: []
        qbs.install: qbs.targetOS.contains("windows")
        qbs.installDir: qbsbuildconfig.appInstallDir
    }

    Group {
        name: "Python executables"
        files: ["../src/3rdparty/python/bin/dmgbuild"]
        fileTags: ["qbs resources"]
        qbs.install: true
        qbs.installDir: qbsbuildconfig.libexecInstallDir
        qbs.installSourceBase: "../src/3rdparty/python/bin"
    }

    Group {
        name: "Python packages"
        prefix: "../src/3rdparty/python/**/"
        files: ["*.py"]
        fileTags: ["qbs resources"]
        qbs.install: true
        qbs.installDir: qbsbuildconfig.resourcesInstallDir + "/share/qbs/python"
        qbs.installSourceBase: "../src/3rdparty/python/lib/python2.7/site-packages"
    }

    Group {
        name: "Modules and imports"
        files: ["qbs/**/*"]
        fileTags: ["qbs resources"]
        qbs.install: true
        qbs.installDir: qbsbuildconfig.resourcesInstallDir + "/share"
        qbs.installSourceBase: "."
    }

    Group {
        name: "Examples as resources"
        files: ["../examples/**/*"]
        fileTags: []
        qbs.install: true
        qbs.installDir: qbsbuildconfig.resourcesInstallDir + "/share/qbs"
        qbs.installSourceBase: ".."
    }

    Rule {
        inputs: ["qbs resources"]
        Artifact {
            filePath: FileInfo.joinPaths(
                          project.buildDirectory,
                          input.moduleProperty("qbs", "installDir"),
                          FileInfo.relativePath(input.moduleProperty("qbs", "installSourceBase"),
                                                input.filePath))
            fileTags: ["copied qbs resources"]
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "Copying resource " + input.fileName + " to build directory.";
            cmd.highlight = "codegen";
            cmd.sourceCode = function() { File.copy(input.filePath, output.filePath); }
            return cmd;
        }
    }
}
