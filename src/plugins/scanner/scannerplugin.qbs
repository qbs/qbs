import qbs 1.0

DynamicLibrary {
    Depends { name: "cpp" }
    Depends { name: "qt.core" }
    destinationDirectory: "plugins"
    Group {
        fileTagsFilter: ["dynamiclibrary"]
        qbs.install: true
        qbs.installDir: "plugins"
    }
}

