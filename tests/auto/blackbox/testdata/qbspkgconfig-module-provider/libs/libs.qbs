import qbs.FileInfo

Project {
    property bool isBundle: false

    DynamicLibrary {
        Depends { name: "cpp" }
        Depends { name: "bundle" }
        Depends { name: "Exporter.pkgconfig" }
        Exporter.pkgconfig.versionEntry: "1.0"
        name: "libA"
        bundle.isBundle: project.isBundle
        bundle.publicHeaders: ["libA.h"]
        files: "libA.cpp"
        cpp.defines: {
            var result = [];
            if (project.isBundle)
                result.push("MYLIB_FRAMEWORK");
            return result;
        }
        qbs.installPrefix: "/usr"
        install: true
        installImportLib: true
        installDir: "lib"
        Group {
            files: ["libA.h"]
            qbs.install: !project.isBundle
            qbs.installDir: FileInfo.joinPaths("include", product.name)
        }
        Group {
            fileTagsFilter: ["Exporter.pkgconfig.pc"]
            qbs.install: !project.isBundle
            qbs.installDir: FileInfo.joinPaths("share", "pkgconfig")
        }
        Export {
            Depends { name: "cpp" }
            cpp.defines: ["THE_MAGIC_DEFINE"]
            cpp.includePaths: [FileInfo.joinPaths(exportingProduct.qbs.installPrefix, "include")]
            cpp.libraryPaths: [FileInfo.joinPaths(exportingProduct.qbs.installPrefix, "lib")]
        }
    }
}
