import qbs
import qbs.File
import qbs.FileInfo

Product {
    Depends { name: "qbsbuildconfig" }
    Depends { name: "Qt"; submodules: ["core", "gui", "network", "printsupport", "widgets", "xml"] }
    Depends { name: "Qt.test"; condition: project.withTests === true }
    Depends { name: "Qt.script"; condition: !qbsbuildconfig.useBundledQtScript; required: false }
    Depends {
        name: "Qt";
        submodules: [ "dbus", "xcb_qpa_lib-private" ];
        required: false
    }

    condition: {
        if (!qbsbuildconfig.enableBundledQt)
            return false;
        if (Qt.core.staticBuild)
            throw("Cannot bundle static Qt libraries");
        return true;
    }

    readonly property string qtDebugLibrarySuffix: {
        if (Qt.core.qtBuildVariant !== "debug")
            return "";
        if (qbs.targetOS.contains("windows"))
            return "d";
        if (qbs.targetOS.contains("darwin"))
            return "_debug";
        return "";
    }

    Group {
        name: "qt.conf"
        files: ["qt.conf"]
        qbs.install: true
        qbs.installDir: qbsbuildconfig.appInstallDir
    }

    Group {
        name: "Qt libraries"
        files: {
            function getLibsForQtModule(mod) {
                if (mod === "script" && !Qt[mod].present)
                    return [];
                if ((mod !== "core") && !Qt[mod].hasLibrary)
                    return [];
                if (Qt[mod].isStaticLibrary)
                    return [];

                var list = [];
                if (qbs.targetOS.contains("windows")) {
                    var basename = FileInfo.baseName(Qt[mod].libNameForLinker);
                    var dir = Qt.core.binPath;
                    list.push(dir + "/" + basename + ".dll");

                } else if (qbs.targetOS.contains("linux")) {
                    var fp = Qt[mod].libFilePath;
                    var basename = FileInfo.baseName(fp);
                    var dir      = FileInfo.path(fp);
                    list.push(dir + "/" + basename + ".so");
                    list.push(dir + "/" + basename + ".so." + Qt.core.versionMajor);
                    list.push(dir + "/" + basename + ".so." + Qt.core.versionMajor + "." + Qt.core.versionMinor);
                    list.push(fp);

                } else if (Qt.core.frameworkBuild) {
                    var fp = Qt[mod].libFilePathRelease;
                    var fpd = fp + "_debug";

                    if (qtDebugLibrarySuffix)
                        list.push(fpd);

                    var suffix = ".framework/";
                    var frameworkPath = fp.substr(0, fp.lastIndexOf(suffix) + suffix.length - 1);
                    var versionsPath = frameworkPath + "/Versions";
                    var versionPath = versionsPath + "/" + Qt.core.versionMajor;
                    list.push(frameworkPath + "/Resources");
                    list.push(versionPath + "/Resources/Info.plist");
                    list.push(versionPath + "/" + FileInfo.fileName(fp));
                    if (qtDebugLibrarySuffix)
                        list.push(versionPath + "/" + FileInfo.fileName(fpd));
                    if (qbsbuildconfig.installApiHeaders) {
                        list.push(frameworkPath + "/Headers");
                        list.push(versionPath + "/Headers/**");
                    }
                }
                return list;
            }

            var qtModules = Object.getOwnPropertyNames(Qt);
            var libraries = Array.prototype.concat.apply([], qtModules.map(getLibsForQtModule));

            // Qt might be bundled with additional libraries
            if (qbs.targetOS.contains("linux")) {
                var dir = FileInfo.path(Qt.core.libFilePathRelease);
                var addons = [ "libicui18n", "libicuuc", "libicudata" ];
                addons.forEach(function(lib) {
                    var fp = dir + "/" + lib + ".so";
                    if (File.exists(fp))
                        libraries.push(fp + "*");
                });
            }

            return libraries;
        }

        qbs.install: true
        qbs.installDir: qbsbuildconfig.libInstallDir
        qbs.installSourceBase: qbs.targetOS.contains("windows") ? Qt.core.binPath : Qt.core.libPath
    }

    Group {
        name: "Windows Plugins"
        condition: qbs.targetOS.contains("windows")
        prefix: Qt.core.pluginPath + "/"
        files: [
            "platforms/qwindows" + qtDebugLibrarySuffix + cpp.dynamicLibrarySuffix,
            "styles/qwindowsvistastyle" + qtDebugLibrarySuffix + cpp.dynamicLibrarySuffix
        ]
        qbs.install: true
        qbs.installDir: "plugins"
        qbs.installSourceBase: prefix
    }

    Group {
        name: "macOS Plugins"
        condition: qbs.targetOS.contains("darwin")
        prefix: Qt.core.pluginPath + "/"
        files: [
            "platforms/libqcocoa" + qtDebugLibrarySuffix + cpp.dynamicLibrarySuffix,
            "styles/libqmacstyle" + qtDebugLibrarySuffix + cpp.dynamicLibrarySuffix
        ]
        qbs.install: true
        qbs.installDir: "plugins"
        qbs.installSourceBase: prefix
    }

    Group {
        name: "Linux Plugins"
        condition: qbs.targetOS.contains("linux")
        prefix: Qt.core.pluginPath + "/"
        files: [
            "platforms/libqxcb" + cpp.dynamicLibrarySuffix,
            "platformthemes/libqgtk3" + cpp.dynamicLibrarySuffix
        ]
        qbs.install: true
        qbs.installDir: "plugins"
        qbs.installSourceBase: prefix
    }

    Group {
        name: "MinGW Runtime DLLs"
        condition: qbs.targetOS.contains("windows") && qbs.toolchain.contains("mingw")

        files: {
            var libFileGlobs = [
                "*libgcc_s*.dll",
                "*libstdc++-6.dll",
                "*libwinpthread-1.dll"
            ];
            var searchPaths = cpp.compilerLibraryPaths;
            return Array.prototype.concat.apply([], searchPaths.map(function(path) {
                return libFileGlobs.map(function(glob) {
                    return path + "/" + glob;
                });
            }));
        }

        qbs.install: true
        qbs.installDir: "bin"
    }
}
