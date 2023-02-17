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
            var result = qbs.targetPlatform === Host.platform();
            if (!result)
                console.info("targetPlatform differs from hostPlatform");
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

        Group {
            fileTagsFilter: product.type
            qbs.install: true
        }

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

        Group {
            fileTagsFilter: product.type
            qbs.install: true
        }

        files: ["theplugin.cpp"]

        Group {
            files: ["metadata.json"]
            fileTags: ["qt_plugin_metadata"]
        }
    }
}
