import qbs.Host
import qbs.Utilities

Project {
    QtApplication {
        condition: {
            if (Utilities.versionCompare(Qt.core.version, "5.0") < 0) {
                // qt4 moc can't be used with pluginMetaData
                console.info("using qt4");
                return false;
            }
            var result = qbs.targetPlatform === Host.platform() && qbs.architecture === Host.architecture();
            if (!result)
                console.info("target platform/arch differ from host platform/arch");
            return result;
        }

        name: "app"
        consoleApplication: true

        Depends { name: "thePlugin" }

        cpp.cxxLanguageVersion: "c++11"

        Properties {
            condition: qbs.targetOS.includes("unix")
            cpp.rpaths: [cpp.rpathOrigin]
        }
        config.install.binariesDirectory: ""

        files: ["app.cpp"]
    }

    Library {
        type: Qt.core.staticBuild ? "staticlibrary" : "dynamiclibrary"
        name: "thePlugin"

        Depends { name: "cpp" }
        Depends { name: "Qt.core" }

        Properties {
            condition: qbs.targetOS.includes("darwin")
            bundle.isBundle: false
        }
        cpp.defines: [Qt.core.staticBuild ? "QT_STATICPLUGIN" : "QT_PLUGIN"]
        cpp.cxxLanguageVersion: "c++11"
        cpp.sonamePrefix: qbs.targetOS.includes("darwin") ? "@rpath" : undefined
        cpp.includePaths: ["."]
        Qt.core.pluginMetaData: ["theKey=theValue"]
        config.install.dynamicLibrariesDirectory: ""
        config.install.staticLibrariesDirectory: ""

        files: ["theplugin.cpp"]

        Group {
            files: ["metadata.json"]
            fileTags: ["qt_plugin_metadata"]
        }
    }
}
