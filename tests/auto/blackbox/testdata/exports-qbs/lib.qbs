import qbs.FileInfo

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
    install: true
    installImportLib: true
    installDir: "lib"
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
        Properties {
            condition: true
            prefixMapping: [{
                prefix: includeDir,
                replacement: FileInfo.joinPaths(qbs.installPrefix, product.headersInstallDir)
            }]
        }
    }
}
