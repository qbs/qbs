import qbs 1.0

DynamicLibrary {
    Depends { name: "cpp" }
    Depends { name: "Qt.core" }
    Depends { name: "qbsbuildconfig" }
    cpp.cxxLanguageVersion: "c++11"
    destinationDirectory: qbsbuildconfig.libDirName + "/qbs/plugins"
    Group {
        fileTagsFilter: ["dynamiclibrary"]
        qbs.install: true
        qbs.installDir: qbsbuildconfig.pluginsInstallDir + "/qbs/plugins"
    }
    bundle.isBundle: false
}
