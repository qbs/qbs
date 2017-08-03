import qbs

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
    // TODO: Use Utilities.cStringQuote
    cpp.defines: base.concat(['SRCDIR="' + path + '"'])
}
