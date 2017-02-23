import qbs
import qbs.FileInfo
import qbs.TextFile

Project {
    property string hostProfile
    property string targetProfile
    Product {
        name: "p1"
        type: "output"
        profiles: [project.targetProfile, project.hostProfile]
        Group {
            files: "host+target.input"
            fileTags: "input"
        }
        Group {
            fileTagsFilter: "output"
            qbs.install: true
            qbs.installDir: profile
        }
    }
    Product {
        name: "p2"
        type: "output"
        profiles: project.hostProfile
        Group {
            files: "host-tool.input"
            fileTags: "input"
        }
        Group {
            fileTagsFilter: "output"
            qbs.install: true
            qbs.installDir: profile
        }
    }

    Rule {
        inputs: "input"
        Artifact {
            filePath: FileInfo.baseName(input.fileName) + ".output"
            fileTags: "output"
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generating " + output.fileName;
            cmd.sourceCode = function() {
               var file = new TextFile(output.filePath, TextFile.WriteOnly);
               file.write(product.moduleProperty("qbs", "architecture"));
               file.close();
            }
            return cmd;
        }
    }
}
