import qbs
import qbs.File
import qbs.FileInfo

Product {
    name: "qbs resources"
    type: ["copied qbs resources"]
    Group {
        name: "Modules and imports"
        files: ["qbs/**/*"]
        fileTags: ["qbs resources"]
        qbs.install: true
        qbs.installDir: project.resourcesInstallDir + "/share"
        qbs.installSourceBase: "."
    }

    Group {
        name: "Examples as resources"
        files: ["../examples/**/*"]
        fileTags: []
        qbs.install: true
        qbs.installDir: project.resourcesInstallDir + "/share/qbs"
        qbs.installSourceBase: ".."
    }

    Rule {
        inputs: ["qbs resources"]
        Artifact {
            filePath: FileInfo.joinPaths(project.buildDirectory, project.resourcesInstallDir,
                    "share", FileInfo.relativePath(product.sourceDirectory, input.filePath))
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
