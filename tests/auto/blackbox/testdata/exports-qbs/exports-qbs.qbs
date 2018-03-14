import qbs
import qbs.FileInfo

import "helper.js" as Helper

Project {
    property string installPrefix: "/usr"
    Product {
        name: "local"
        Export {
            property bool dummy
            Depends { name: "cpp" }
            cpp.includePaths: ["/somelocaldir/include"]
        }
    }

    CppApplication {
        name: "MyTool"
        consoleApplication: true
        property stringList toolTags: ["MyTool.tool"]
        Depends { name: "Exporter.qbs" }
        Exporter.qbs.artifactTypes: ["installable", "blubb"]
        files: ["tool.cpp"]
        qbs.installPrefix: project.installPrefix
        Group {
            files: ["helper.js"]
            qbs.install: true
            qbs.installDir: "qbs/modules/MyTool"
        }

        Group {
            fileTagsFilter: ["application"]
            qbs.install: true
            qbs.installDir: "bin"
            fileTags: toolTags
        }
        Group {
            fileTagsFilter: ["Exporter.qbs.module"]
            qbs.installDir: "qbs/modules/MyTool"
        }

        Export {
            property stringList toolTags: product.toolTags
            property stringList outTags: [importingProduct.outTag]
            Rule {
                inputs: Helper.toolInputs()
                explicitlyDependsOn: toolTags

                outputFileTags: parent.outTags
                outputArtifacts: [{
                        filePath: FileInfo.completeBaseName(input.fileName),
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

    DynamicLibrary {
        name: "MyLib"
        multiplexByQbsProperties: ["buildVariants"]
        aggregate: false
        qbs.buildVariants: ["debug", "release"]
        qbs.installPrefix: project.installPrefix
        Depends { name: "cpp" }
        Depends { name: "Exporter.qbs" }
        Exporter.qbs.fileName: name + "_" + qbs.buildVariant + ".qbs"
        Exporter.qbs.excludedDependencies: ["local"]
        Exporter.qbs.additionalContent: "    condition: qbs.buildVariant === '"
                                        + qbs.buildVariant + "'"
        property string headersInstallDir: "include"
        cpp.defines: ["MYLIB_BUILD"]
        cpp.variantSuffix: qbs.buildVariant === "debug" ? "d" : ""
        Properties {
            condition: qbs.targetOS.contains("darwin")
            bundle.isBundle: false
        }
        files: ["mylib.cpp"]
        property var config: ({feature_x: false, feature_y: true})
        Group {
            name: "API headers"
            files: ["mylib.h"]
            qbs.install: true
            qbs.installDir: headersInstallDir
        }
        Group {
            fileTagsFilter: ["dynamiclibrary", "dynamiclibrary_import"]
            qbs.install: true
            qbs.installDir: "lib"
        }
        Group {
            fileTagsFilter: ["Exporter.qbs.module"]
            qbs.install: true
            qbs.installDir: "qbs/modules/MyLib"
        }

        Export {
            Depends { name: "cpp" }
            property string includeDir: product.sourceDirectory
            property var config: product.config
            Properties {
                condition: true
                cpp.includePaths: [includeDir]
                cpp.dynamicLibraries: []
            }
            cpp.dynamicLibraries: ["nosuchlib"]
            Depends { name: "local" }
            local.dummy: true
            prefixMapping: [{
                    prefix: includeDir,
                    replacement: FileInfo.joinPaths(qbs.installPrefix, product.headersInstallDir)
                }]
        }
    }

    references: ["consumer.qbs"]
}
