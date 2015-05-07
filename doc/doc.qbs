import qbs 1.0
import QbsFunctions

Product {
    name: "documentation"
    builtByDefault: false
    type: "qch"
    Depends { name: "Qt.core" }

    files: [
        "qbs.qdoc",
        "config/*.qdocconf",
        "reference/**/*",
    ]
    Group {
        name: "main qdocconf file"
        files: "qbs.qdocconf"
        fileTags: "qdocconf-main"
    }

    property string versionTag: QbsFunctions.qbsVersion().replace(/\.|-/g, "")
    Qt.core.qdocEnvironment: [
        "QBS_VERSION=" + QbsFunctions.qbsVersion(),
        "SRCDIR=" + path,
        "QT_INSTALL_DOCS=" + Qt.core.docPath,
        "QBS_VERSION_TAG=" + versionTag
    ]

    Group {
        fileTagsFilter: ["qdoc-output"]
        qbs.install: true
        qbs.installDir: "share/doc/qbs/html"
        qbs.installSourceBase: Qt.core.qdocOutputDir
    }
}
