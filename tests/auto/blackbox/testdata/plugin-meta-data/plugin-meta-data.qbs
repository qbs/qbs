import qbs

Project {
    QtApplication {
        consoleApplication: true

        Depends { name: "thePlugin" }

        cpp.cxxLanguageVersion: "c++11"
        cpp.rpaths: qbs.targetOS.contains("darwin") ? ["@loader_path"] : ["$ORIGIN"]

        Group {
            fileTagsFilter: product.type
            qbs.install: true
        }

        files: ["app.cpp"]
    }

    DynamicLibrary {
        name: "thePlugin"

        Depends { name: "cpp" }
        Depends { name: "Qt.core" }

        bundle.isBundle: false
        cpp.defines: ["QT_PLUGIN"]
        cpp.cxxLanguageVersion: "c++11"
        cpp.sonamePrefix: qbs.targetOS.contains("darwin") ? "@rpath" : undefined
        Qt.core.pluginMetaData: ["theKey=theValue"]

        Group {
            fileTagsFilter: product.type
            qbs.install: true
        }

        files: ["theplugin.cpp"]
    }
}
