import qbs.Utilities

QbsUnittest {
    Depends { name: "qbsversion" }
    Depends { name: "qbsconsolelogger" }

    testName: "language"
    condition: qbsbuildconfig.enableUnitTests

    Properties {
        condition: qbs.toolchain.contains("mingw")
        cpp.cxxFlags: "-Wno-maybe-uninitialized"
    }

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
