import qbs 1.0

Project {
    property bool enableUnitTests: false
    property bool enableRPath: true

    references: [
        "src/src.qbs",
        "doc/doc.qbs",
        "share/share.qbs",
        "tests/auto/auto.qbs"
    ]
}
