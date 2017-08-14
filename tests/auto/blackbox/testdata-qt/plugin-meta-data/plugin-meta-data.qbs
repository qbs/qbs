import qbs

Project {
    QtApplication {
        consoleApplication: true

        Depends { name: "thePlugin" }

        cpp.cxxLanguageVersion: "c++11"
        cpp.rpaths: qbs.targetOS.contains("darwin") ? ["@loader_path/"] : ["$ORIGIN"]

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
            condition: qbs.targetOS.contains("darwin")
            bundle.isBundle: false
        }
        cpp.defines: [Qt.core.staticBuild ? "QT_STATICPLUGIN" : "QT_PLUGIN"]
        cpp.cxxLanguageVersion: "c++11"
        cpp.sonamePrefix: qbs.targetOS.contains("darwin") ? "@rpath" : undefined
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
