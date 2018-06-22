import qbs.Utilities

CppApplication {
    name: "app"
    consoleApplication: true
    Depends { name: "Qt.core" }
    Depends { name: "Qt.quick" }
    Depends { name: "Qt.qml" }
    install: true
    installDir: ""
    qbs.installPrefix: ""
    Qt.qml.generateCacheFiles: true
    Qt.qml.cacheFilesInstallDir: "data"

    files: [
        "main.cpp",
        "MainForm.ui.qml",
        "main.qml",
        "stuff.js"
    ]

    // Install the C++ sources to tell the blackbox test that Qt.qmlcache is not available.
    Group {
        condition: !Qt.qml.cachingEnabled
        fileTagsFilter: ["cpp"]
        qbs.install: true
        qbs.installDir: "data"
    }

    Probe {
        id: qtVersionProbe
        property string qtVersion: Qt.core.version
        configure: {
            console.info("qmlcachegen must work: "
                         + (Utilities.versionCompare(qtVersion, "5.11") >= 0))
        }
    }
}
