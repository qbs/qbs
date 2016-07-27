import qbs

Project {
    QtApplication {
        consoleApplication: true

        Depends { name: "thePlugin" }

        cpp.cxxLanguageVersion: "c++11"

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

        cpp.defines: ["QT_PLUGIN"]
        cpp.cxxLanguageVersion: "c++11"
        Qt.core.pluginMetaData: ["theKey=theValue"]

        Group {
            fileTagsFilter: product.type
            qbs.install: true
        }

        files: ["theplugin.cpp"]
    }
}
