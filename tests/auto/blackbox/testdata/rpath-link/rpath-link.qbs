import qbs.FileInfo

Project {
    property bool enableConsumer: false
    property bool enableRPathLink: false

    DynamicLibrary {
        name: "indirect"

        Probe {
            id: targetInfo
            property bool isUnixGcc: qbs.targetOS.includes("unix")
                && !qbs.targetOS.contains("darwin") && qbs.toolchain.includes("gcc")
            configure: {
                console.info("is gcc on unix: " + isUnixGcc)
            }
        }

        Depends { name: "cpp" }

        cpp.defines: "INDIRECT_LIBRARY"

        files: [
            "indirect.cpp",
            "indirect.h",
            "indirect_global.h",
        ]

        qbs.installPrefix: ""
        installDir: "/lib"
    }

    DynamicLibrary {
        name: "direct"

        Depends { name: "cpp" }
        Depends { name: "indirect" }
        Depends { name: "Exporter.qbs" }

        cpp.defines: "DIRECT_LIBRARY"

        Group {
            name: "public headers"
            files: [
                "direct.h",
                "direct_global.h",
            ]
            qbs.install: true
            qbs.installDir: headersInstallDir
        }

        files: "direct.cpp"

        Group {
            fileTagsFilter: ["Exporter.qbs.module"]
            qbs.installDir: "/qbs/modules/direct-installed"
        }

        qbs.installPrefix: ""
        installDir: "/lib"
        property string headersInstallDir: "/include"

        Export {
            Depends { name: "cpp" }
            cpp.includePaths: qbs.installRoot + '/' + exportingProduct.headersInstallDir
        }
    }

    CppApplication {
        name: "consumer"
        condition: enableConsumer

        qbsSearchPaths: qbs.installRoot + "/qbs"
        Depends { name: "direct-installed" }

        Properties {
            condition: project.enableRPathLink
            cpp.rpathLinkDirs: qbs.installRoot + "/lib"
        }

        files: "main.cpp"
    }
}
