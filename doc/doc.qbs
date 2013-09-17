import qbs 1.0
import "../version.js" as Version

Product {
    name: "documentation"
    type: "qch"
    Depends { name: "Qt.core" }

    files: [
        "qbs.qdoc",
        "config/*.qdocconf",
        "reference/**/*",
    ]
    Group {
        name: "main qdocconf file"
        files: Qt.core.versionMajor >= 5 ? "qbs.qdocconf" : "qbs-qt4.qdocconf"
        fileTags: "qdocconf-main"
    }

    property string versionTag: Version.qbsVersion().replace(/\.|-/g, "")
    Qt.core.qdocQhpFileName: "qbs.qhp"
    Qt.core.qdocEnvironment: [
        "QBS_VERSION=" + Version.qbsVersion(),
        "SRCDIR=" + path,
        "QT_INSTALL_DOCS=" + Qt.core.docPath,
        "QBS_VERSION_TAG=" + versionTag
    ]

    Group {
        fileTagsFilter: "qdoc-html"
        qbs.install: true
        qbs.installDir: "share/doc/qbs"
    }
}
