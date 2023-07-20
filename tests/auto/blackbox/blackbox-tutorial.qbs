import qbs.Utilities

QbsAutotest {
    testName: "blackbox-tutorial"
    Depends { name: "qbs_app" }
    Depends { name: "qbs-setup-toolchains" }
    Group {
        name: "testdata"
        prefix: "../../../tutorial/"
        files: ["**/*"]
        fileTags: []
    }
    files: [
        "../shared.h",
        "tst_blackboxtutorial.cpp",
        "tst_blackboxtutorial.h",
        "tst_blackboxbase.cpp",
        "tst_blackboxbase.h",
    ]
    cpp.defines: base.concat(["SRCDIR=" + Utilities.cStringQuote(path)])
}
