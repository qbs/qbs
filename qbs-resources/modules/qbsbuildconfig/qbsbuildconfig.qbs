import qbs
import qbs.FileInfo
import qbs.Utilities

Module {
    Depends {
        condition: project.withCode
        name: "cpp"
    }
    property bool enableAddressSanitizer: false
    property bool enableUbSanitizer: false
    property bool enableThreadSanitizer: false
    property bool enableUnitTests: false
    property bool enableProjectFileUpdates: false
    property bool enableRPath: true
    property bool installApiHeaders: true
    property bool enableBundledQt: false
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
        if (enableRPath && project.withCode && cpp.rpathOrigin && product.targetInstallDir) {
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

    Properties {
        condition: project.withCode && qbs.toolchain.contains("gcc")
        cpp.cxxFlags: {
            var flags = [];
            if (enableAddressSanitizer)
                flags.push("-fno-omit-frame-pointer");
            function isClang() { return qbs.toolchain.contains("clang"); }
            function versionAtLeast(v) {
                return Utilities.versionCompare(cpp.compilerVersion, v) >= 0;
            };
            if ((!isClang() && versionAtLeast("9"))
                    || (isClang() && !qbs.hostOS.contains("darwin") && versionAtLeast("10"))) {
                flags.push("-Wno-deprecated-copy", "-Wno-constant-logical-operand");
            }
            return flags;
        }
        cpp.driverFlags: {
            var flags = [];
            if (enableAddressSanitizer)
                flags.push("-fsanitize=address");
            if (enableUbSanitizer)
                flags.push("-fsanitize=undefined");
            if (enableThreadSanitizer)
                flags.push("-fsanitize=thread");
            return flags;
        }
    }
}
