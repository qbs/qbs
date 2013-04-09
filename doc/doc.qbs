import qbs 1.0

Product {
    name: "documentation"
    type: "qch"
    Depends { name: "qt.core" }

    files: [
        "qbs.qdoc",
        "config/*.qdocconf",
        "items/*",
    ]
    Group {
        name: "main qdocconf file"
        files: qt.core.versionMajor >= 5 ? "qbs.qdocconf" : "qbs-qt4.qdocconf"
        fileTags: "qdocconf-main"
    }

    property string versionTag: project.version.replace(/\.|-/g, "")
    qt.core.qdocQhpFileName: "qbs.qhp"
    qt.core.qdocEnvironment: [
        "QBS_VERSION=" + project.version,
        "SRCDIR=.",
        "QBS_VERSION_TAG=" + versionTag
    ]

    Group {
        fileTagsFilter: "qdoc-html"
        qbs.install: true
        qbs.installDir: "share/doc/qbs"
    }
}
