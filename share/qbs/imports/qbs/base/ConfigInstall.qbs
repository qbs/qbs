Module {
    Depends { name: "installpaths" }

    property bool install: true

    property string binariesDirectory: installpaths.bin
    property string applicationsDirectory: installpaths.applications

    property bool dynamicLibraries: install
    property string dynamicLibrariesDirectory: qbs.targetOS.contains("windows")
        ? installpaths.bin : installpaths.lib

    property bool staticLibraries: false
    property string staticLibrariesDirectory: installpaths.lib

    property bool frameworks: install
    property string frameworksDirectory: installpaths.library + "/" + installpaths.frameworks

    property bool importLibraries: false
    property string importLibrariesDirectory: installpaths.lib

    property bool plugins: install
    property string pluginsDirectory: installpaths.plugins

    property bool loadableModules: install
    property string loadableModulesDirectory:
        installpaths.library + "/" + installpaths.topLevelProjectName + "/PlugIns"

    property bool debugInformation: install
    property string debugInformationDirectory
}
