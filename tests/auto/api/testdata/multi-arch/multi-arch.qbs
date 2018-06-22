import qbs.FileInfo
import qbs.TextFile

Project {
    property string hostProfile
    property string targetProfile
    Product {
        property stringList myProfiles
        name: "p1"
        type: "output"
        qbs.profiles: myProfiles ? myProfiles : [project.targetProfile, project.hostProfile]
        Group {
            files: "host+target.input"
            fileTags: "input"
        }
        qbs.installPrefix: ""
        Group {
            fileTagsFilter: "output"
            qbs.install: true
            qbs.installDir: qbs.profile
        }
    }
    Product {
        name: "p2"
        type: "output"
        qbs.profiles: [project.hostProfile]
        Group {
            files: "host-tool.input"
            fileTags: "input"
        }
        qbs.installPrefix: ""
        Group {
            fileTagsFilter: "output"
            qbs.install: true
            qbs.installDir: qbs.profile
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
