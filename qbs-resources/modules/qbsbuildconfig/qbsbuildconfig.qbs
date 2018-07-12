import qbs
import qbs.FileInfo

Module {
    Depends { name: "cpp" }
    property bool enableUnitTests: false
    property bool enableProjectFileUpdates: false
    property bool enableRPath: true
    property bool installApiHeaders: true
    property bool enableBundledQt: true
    property bool useBundledQtScript: false
    property bool staticBuild: false
    property string libDirName: "lib"
    property string appInstallDir: "bin"
    property string libInstallDir: qbs.targetOS.contains("windows") ? "bin" : libDirName
    property string importLibInstallDir: libDirName
    property string libexecInstallDir: qbs.targetOS.contains("windows") ? appInstallDir
                                                                        : "libexec/qbs"
    property string systemSettingsDir
    property bool installManPage: qbs.targetOS.contains("unix")
    property bool installHtml: true
    property bool installQch: false
    property bool generatePkgConfigFiles: installApiHeaders && qbs.targetOS.contains("unix")
                                          && !qbs.targetOS.contains("darwin")
    property bool generateQbsModules: installApiHeaders
    property string docInstallDir: "share/doc/qbs/html"
    property string pkgConfigInstallDir: FileInfo.joinPaths(libDirName, "pkgconfig")
    property string qbsModulesBaseDir: FileInfo.joinPaths(libDirName, "qbs", "modules")
    property string relativeLibexecPath: "../" + libexecInstallDir
    property string relativePluginsPath: "../" + libDirName
    property string relativeSearchPath: ".."
    property stringList libRPaths: {
        if (enableRPath && cpp.rpathOrigin && product.targetInstallDir) {
            return [FileInfo.joinPaths(cpp.rpathOrigin, FileInfo.relativePath(
                                           FileInfo.joinPaths('/', product.targetInstallDir),
                                           FileInfo.joinPaths('/', libDirName)))];
        }
        return [];
    }
    property string resourcesInstallDir: ""
    property string pluginsInstallDir: libDirName + "/qbs/plugins"
    property string qmlTypeDescriptionsInstallDir: FileInfo.joinPaths(resourcesInstallDir,
                                                                  "share/qbs/qml-type-descriptions")
}
