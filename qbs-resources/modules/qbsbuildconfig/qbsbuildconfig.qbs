import qbs

Module {
    property bool enableUnitTests: false
    property bool enableProjectFileUpdates: false
    property bool enableRPath: true
    property bool installApiHeaders: true
    property string libDirName: "lib"
    property string appInstallDir: "bin"
    property string libInstallDir: qbs.targetOS.contains("windows") ? "bin" : libDirName
    property string importLibInstallDir: libDirName
    property string libexecInstallDir: qbs.targetOS.contains("windows") ? appInstallDir
                                                                        : "libexec/qbs"
    property bool installHtml: true
    property bool installQch: false
    property string docInstallDir: "share/doc/qbs/html"
    property string relativeLibexecPath: "../" + libexecInstallDir
    property string relativePluginsPath: "../" + libDirName
    property string relativeSearchPath: ".."
    property stringList libRPaths: {
        if (!enableRPath)
            return undefined;
        if (qbs.targetOS.contains("linux"))
            return ["$ORIGIN/../" + libDirName];
        if (qbs.targetOS.contains("macos"))
            return ["@loader_path/../" + libDirName]
    }
    property string resourcesInstallDir: ""
    property string pluginsInstallDir: libDirName
}
