import qbs
import qbs.Utilities

QbsAutotest {
    Depends { name: "qbsversion" }
    Depends {
        name: "Qt.script"
        condition: !qbsbuildconfig.useBundledQtScript
        required: false
    }

    testName: "language"
    condition: qbsbuildconfig.enableUnitTests
    files: [
        "tst_language.cpp",
        "tst_language.h"
    ]

    cpp.defines: base.concat([
        "QBS_VERSION=" + Utilities.cStringQuote(qbsversion.version),
        "SRCDIR=" + Utilities.cStringQuote(path)
    ])

    Group {
        name: "testdata"
        prefix: "testdata/"
        files: ["**/*"]
        fileTags: []
    }
}
