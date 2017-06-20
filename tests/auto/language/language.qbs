import qbs

QbsAutotest {
    Depends { name: "qbsversion" }
    Depends { name: "Qt.script" }

    testName: "language"
    condition: qbsbuildconfig.enableUnitTests
    files: [
        "tst_language.cpp",
        "tst_language.h"
    ]

    // TODO: Use Utilities.cStringQuote
    cpp.defines: base.concat([
        'QBS_VERSION="' + qbsversion.version + '"',
        "SRCDIR=\"" + path + "\""
    ])

    Group {
        name: "testdata"
        prefix: "testdata/"
        files: ["**/*"]
        fileTags: []
    }
}
