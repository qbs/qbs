import qbs.Utilities

QbsAutotest {
    testName: "blackbox-java"
    Depends { name: "qbs_app" }
    Depends { name: "qbs-setup-toolchains" }
    Group {
        name: "testdata"
        prefix: "testdata-java/"
        files: ["**/*"]
        fileTags: []
    }
    files: [
        "../shared.h",
        "tst_blackboxbase.cpp",
        "tst_blackboxbase.h",
        "tst_blackboxjava.cpp",
        "tst_blackboxjava.h",
    ]
    cpp.defines: base.concat(["SRCDIR=" + Utilities.cStringQuote(path)])
}
