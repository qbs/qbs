import qbs.Utilities

QbsAutotest {
    testName: "blackbox"
    Depends { name: "qbs_app" }
    Depends { name: "qbs-setup-toolchains" }
    Group {
        name: "find"
        prefix: "find/"
        files: ["**/*"]
        fileTags: []
    }
    Group {
        name: "testdata"
        prefix: "testdata/"
        files: ["**/*"]
        fileTags: []
    }
    files: [
        "../shared.h",
        "tst_blackboxbase.cpp",
        "tst_blackboxbase.h",
        "tst_blackbox.cpp",
        "tst_blackbox.h",
    ]
    cpp.defines: base.concat(["SRCDIR=" + Utilities.cStringQuote(path)])
        .concat(qbsbuildconfig.enableUnitTests ? ["QBS_ENABLE_UNIT_TESTS"] : [])
}
