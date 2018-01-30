import qbs
import qbs.FileInfo

Product {
    Depends { name: "qbsbuildconfig" }
    Depends { name: "Qt"; submodules:
            ["core", "gui", "network", "printsupport", "script", "widgets", "xml"] }

    property bool deployQt: qbsbuildconfig.enableBundledQt && qbs.targetOS.contains("macos")
                            && Qt.core.qtConfig.contains("rpath")
    property bool deployDebugLibraries: qbs.buildVariants.contains("debug")

    readonly property string qtDebugLibrarySuffix: {
        if (qbs.targetOS.contains("windows"))
            return "d";
        if (qbs.targetOS.contains("darwin"))
            return "_debug";
        return "";
    }

    Group {
        condition: deployQt && !Qt.core.staticBuild
        name: "qt.conf"
        files: ["qt.conf"]
        qbs.install: true
        qbs.installDir: qbsbuildconfig.appInstallDir
    }

    Group {
        condition: deployQt
        name: "Qt libraries"
        files: !Qt.core.staticBuild ? Array.prototype.concat.apply(
                                          [], Object.getOwnPropertyNames(Qt).map(function(mod) {
            var fp = Qt[mod].libFilePathRelease;
            var fpd = Qt.core.frameworkBuild ? fp + qtDebugLibrarySuffix : Qt[mod].libFilePathDebug;

            var list = [fp];
            if (deployDebugLibraries && qtDebugLibrarySuffix)
                list.push(fpd);

            if (Qt.core.frameworkBuild) {
                var suffix = ".framework/";
                var frameworkPath = fp.substr(0, fp.lastIndexOf(suffix) + suffix.length - 1);
                var versionsPath = frameworkPath + "/Versions";
                var versionPath = versionsPath + "/" + Qt.core.versionMajor;
                list.push(frameworkPath + "/Resources");
                list.push(versionPath + "/Resources/Info.plist");
                list.push(versionPath + "/" + FileInfo.fileName(fp));
                if (deployDebugLibraries && qtDebugLibrarySuffix)
                    list.push(versionPath + "/" + FileInfo.fileName(fpd));
                if (qbsbuildconfig.installApiHeaders) {
                    list.push(frameworkPath + "/Headers");
                    list.push(versionPath + "/Headers/**");
                }
            }

            return list;
        })) : []
        qbs.install: true
        qbs.installDir: qbsbuildconfig.libInstallDir
        qbs.installSourceBase: Qt.core.libPath
    }

    Group {
        condition: deployQt
        prefix: Qt.core.pluginPath + "/"
        name: "QPA plugin"
        files: !Qt.core.staticBuild ? Array.prototype.concat.apply([], [""].concat(
                    deployDebugLibraries && qtDebugLibrarySuffix ? [qtDebugLibrarySuffix] : []).map(
                                                function(suffix) {
            return ["platforms/" + cpp.dynamicLibraryPrefix + (Qt.gui.defaultQpaPlugin || "qcocoa")
                    + suffix + cpp.dynamicLibrarySuffix];
        })) : []
        qbs.install: true
        qbs.installDir: "plugins"
        qbs.installSourceBase: prefix
    }
}
