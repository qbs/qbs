import qbs.FileInfo

Project {
    property bool isStatic: false
    property bool isBundle: false

    property string headersInstallDir: "include"

    Product {
        name: "DllExport"
        Depends { name: "Exporter.cmake" }
        Group {
            name: "API headers"
            files: ["../dllexport.h"]
            qbs.install: true
            qbs.installDir: project.headersInstallDir
        }
        Group {
            fileTagsFilter: ["Exporter.cmake.package"]
            qbs.install: true
            qbs.installDir: "/lib/cmake/DllExport"
        }
        Export {
            Depends { name: "cpp" }
            cpp.includePaths: FileInfo.joinPaths(
                exportingProduct.qbs.installRoot,
                exportingProduct.qbs.installPrefix,
                project.headersInstallDir)
        }
    }

    Library {
        type: project.isStatic ? "staticlibrary" : "dynamiclibrary"
        Depends { name: "cpp" }
        Depends { name: "DllExport" }
        Depends { name: "Exporter.cmake" }
        Exporter.cmake.packageName: "Bar"
        name: "Foo"
        files: ["Foo.cpp"]
        version: "1.2.3"
        cpp.includePaths: "."
        cpp.defines: "FOO_LIB"
        Group {
            name: "API headers"
            files: ["Foo.h"]
            qbs.install: true
            qbs.installDir: project.headersInstallDir
        }
        install: true
        installImportLib: true
        Group {
            fileTagsFilter: ["Exporter.cmake.package"]
            qbs.install: true
            qbs.installDir: "/lib/cmake/Bar"
        }
        Export {
            Depends { name: "cpp" }
            cpp.includePaths: FileInfo.joinPaths(
                exportingProduct.qbs.installRoot,
                exportingProduct.qbs.installPrefix,
                project.headersInstallDir)
            cpp.defines: ["FOO=1"].concat(project.isStatic ? ["FOO_LIB_STATIC"] : [])
            cpp.commonCompilerFlags: "-DOTHER_DEF=1"
            cpp.linkerFlags: exportingProduct.qbs.toolchain.contains("gcc") ? ["-s"] : []
        }

        Depends { name: 'bundle' }
        bundle.isBundle: qbs.targetOS.includes("darwin") && project.isBundle
    }
}
