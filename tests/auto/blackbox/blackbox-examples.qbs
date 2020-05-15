import qbs.Utilities

QbsAutotest {
    testName: "blackbox-examples"
    Depends { name: "qbs_app" }
    Depends { name: "qbs-setup-toolchains" }
    Group {
        name: "testdata"
        prefix: "../../../examples/"
        files: ["**/*"]
        fileTags: []
    }
    files: [
        "../shared.h",
        "tst_blackboxexamples.cpp",
        "tst_blackboxexamples.h",
        "tst_blackboxbase.cpp",
        "tst_blackboxbase.h",
    ]
    cpp.defines: base.concat(["SRCDIR=" + Utilities.cStringQuote(path)])
}
