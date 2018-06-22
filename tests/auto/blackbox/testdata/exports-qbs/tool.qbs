import qbs.FileInfo

import "helper.js" as Helper
import Helper2

CppApplication {
    name: "MyTool"
    consoleApplication: true
    property stringList toolTags: ["MyTool.tool"]
    Depends { name: "Exporter.qbs" }
    Exporter.qbs.artifactTypes: ["installable", "blubb"]
    files: ["tool.cpp"]
    install: true
    qbs.installPrefix: project.installPrefix
    Group {
        files: ["helper.js"]
        qbs.install: true
        qbs.installDir: "qbs/modules/MyTool"
    }
    Group {
        files: ["imports/Helper2/helper2.js"]
        qbs.install: true
        qbs.installDir: "qbs/imports/Helper2"
    }

    Group {
        fileTagsFilter: ["application"]
        fileTags: toolTags
    }
    Group {
        fileTagsFilter: ["Exporter.qbs.module"]
        qbs.installDir: "qbs/modules/MyTool"
    }

    Export {
        property stringList toolTags: product.toolTags
        property stringList outTags: [importingProduct.outTag]
        property var helper2Obj: Helper2
        Rule {
            inputs: Helper.toolInputs()
            explicitlyDependsOnFromDependencies: toolTags

            outputFileTags: parent.outTags
            outputArtifacts: [{
                    filePath: FileInfo.completeBaseName(input.fileName)
                              + product.MyTool.helper2Obj.suffix,
                    fileTags: product.MyTool.outTags
            }]
            prepare: {
                var cmd = new Command(explicitlyDependsOn["MyTool.tool"][0].filePath,
                                      [input.filePath, output.filePath]);
                cmd.description = input.fileName + " -> " + output.fileName;
                return [cmd];
            }
        }
    }
}
