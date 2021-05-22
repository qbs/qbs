import qbs
import qbs.Utilities

QbsUnittest {
    Depends { name: "qbspkgconfig" }
    condition: qbsbuildconfig.enableUnitTests
    testName: "pkgconfig"
    files: ["../shared.h", "tst_pkgconfig.h", "tst_pkgconfig.cpp"]
    cpp.defines: base.concat([
        "SRCDIR=" + Utilities.cStringQuote(path),
    ])

    Group {
        name: "testdata"
        prefix: "testdata/"
        files: ["**/*"]
        fileTags: []
    }
}
