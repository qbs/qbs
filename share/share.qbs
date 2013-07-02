import qbs
import qbs.File

Product {
    name: "qbs resources"
    Group {
        files: ["qbs"]
        qbs.install: true
        qbs.installDir: project.resourcesInstallDir + "/share"
    }

    Transformer {
        inputs: "qbs"
        Artifact {
            fileName: "share/qbs"
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "Copying share/qbs to build directory.";
            cmd.highlight = "codegen";
            cmd.sourceCode = function() { File.copy(input.fileName, output.fileName); }
            return cmd;
        }
    }
}
