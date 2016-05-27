import qbs 1.0

DynamicLibrary {
    Depends { name: "cpp" }
    Depends { name: "Qt.core" }
    cpp.cxxLanguageVersion: "c++11"
    destinationDirectory: project.libDirName + "/qbs/plugins"
    Group {
        fileTagsFilter: ["dynamiclibrary"]
        qbs.install: true
        qbs.installDir: project.pluginsInstallDir + "/qbs/plugins"
    }
    bundle.isBundle: false
}
