import qbs.Utilities

QbsAutotest {
    testName: "blackbox-qt"
    Depends { name: "qbs_app" }
    Depends { name: "qbs-setup-toolchains" }
    Group {
        name: "testdata"
        prefix: "testdata-qt/"
        files: ["**/*"]
        fileTags: []
    }
    files: [
        "../shared.h",
        "tst_blackboxbase.cpp",
        "tst_blackboxbase.h",
        "tst_blackboxqt.cpp",
        "tst_blackboxqt.h",
    ]
    cpp.defines: base.concat(["SRCDIR=" + Utilities.cStringQuote(path)])
}
