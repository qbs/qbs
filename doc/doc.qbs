import qbs 1.0

Product {
    name: "documentation"
    type: "qch"
    Depends { name: "Qt.core" }

    files: [
        "qbs.qdoc",
        "config/*.qdocconf",
        "items/*",
    ]
    Group {
        name: "main qdocconf file"
        files: Qt.core.versionMajor >= 5 ? "qbs.qdocconf" : "qbs-qt4.qdocconf"
        fileTags: "qdocconf-main"
    }

    property string versionTag: project.version.replace(/\.|-/g, "")
    Qt.core.qdocQhpFileName: "qbs.qhp"
    Qt.core.qdocEnvironment: [
        "QBS_VERSION=" + project.version,
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
