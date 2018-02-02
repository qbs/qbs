import qbs
import qbs.FileInfo

Module {
    property bool enableUnitTests: false
    property bool enableProjectFileUpdates: false
    property bool enableRPath: true
    property bool installApiHeaders: true
    property bool enableBundledQt: true
    property string libDirName: "lib"
    property string appInstallDir: "bin"
    property string libInstallDir: qbs.targetOS.contains("windows") ? "bin" : libDirName
    property string importLibInstallDir: libDirName
    property string libexecInstallDir: qbs.targetOS.contains("windows") ? appInstallDir
                                                                        : "libexec/qbs"
    property bool installManPage: qbs.targetOS.contains("unix")
    property bool installHtml: true
    property bool installQch: false
    property string docInstallDir: "share/doc/qbs/html"
    property string relativeLibexecPath: "../" + libexecInstallDir
    property string relativePluginsPath: "../" + libDirName
    property string relativeSearchPath: ".."
    property string rpathOrigin: {
        // qbs < 1.11 compatibility for cpp.rpathOrigin
        if (qbs.targetOS.contains("darwin"))
            return "@loader_path";
        if (qbs.targetOS.contains("unix"))
            return "$ORIGIN";
    }
    property stringList libRPaths: {
        if (enableRPath && rpathOrigin && product.targetInstallDir) {
            if (!FileInfo.cleanPath) {
                // qbs < 1.10 compatibility
                FileInfo.cleanPath = function (a) {
                    if (a.endsWith("/."))
                        return a.slice(0, -2);
                    if (a.endsWith("/"))
                        return a.slice(0, -1);
                    return a;
                }
            }
            return [FileInfo.joinPaths(rpathOrigin, FileInfo.relativePath(
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
