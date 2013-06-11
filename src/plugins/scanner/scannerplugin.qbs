import qbs 1.0

DynamicLibrary {
    Depends { name: "cpp" }
    Depends { name: "Qt.core" }
    destinationDirectory: "lib/qbs/plugins"
    Group {
        fileTagsFilter: ["dynamiclibrary"]
        qbs.install: true
        qbs.installDir: "lib/qbs/plugins"
    }
}

