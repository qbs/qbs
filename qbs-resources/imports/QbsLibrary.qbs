import qbs.FileInfo

QbsLibraryBase {
    Depends { name: "Exporter.pkgconfig"; condition: generatePkgConfigFile }
    Depends { name: "Exporter.qbs"; condition: generateQbsModule }

    Group {
        fileTagsFilter: libType.concat("dynamiclibrary_symlink")
            .concat(qbs.buildVariant === "debug" ? ["debuginfo_dll"] : [])
        qbs.install: install
        qbs.installDir: targetInstallDir
        qbs.installSourceBase: buildDirectory
    }
    targetInstallDir: qbsbuildconfig.libInstallDir
    Group {
        fileTagsFilter: ["dynamiclibrary_import"]
        qbs.install: install
        qbs.installDir: qbsbuildconfig.importLibInstallDir
    }
    Group {
        fileTagsFilter: "Exporter.pkgconfig.pc"
        qbs.install: install
        qbs.installDir: qbsbuildconfig.pkgConfigInstallDir
    }
    Group {
        fileTagsFilter: "Exporter.qbs.module"
        qbs.install: install
        qbs.installDir: FileInfo.joinPaths(qbsbuildconfig.qbsModulesBaseDir, product.name)
    }

    Properties {
        condition: qbs.targetOS.contains("darwin")
        bundle.isBundle: false
        cpp.linkerFlags: ["-compatibility_version", cpp.soVersion]
    }

    Export {
        Depends { name: "cpp" }
        Depends { name: "Qt"; submodules: ["core"] }

        Properties {
            condition: product.hasExporter
            prefixMapping: [{
                prefix: product.sourceDirectory,
                replacement: FileInfo.joinPaths(product.qbs.installPrefix,
                                                product.headerInstallPrefix)
            }]
        }

        cpp.includePaths: [product.sourceDirectory]
        cpp.defines: product.visibilityType === "static" ? ["QBS_STATIC_LIB"] : []
    }
}
