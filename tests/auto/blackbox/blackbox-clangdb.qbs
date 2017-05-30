import qbs

QbsAutotest {
    testName: "blackbox-clangdb"

    Depends { name: "qbs_app" }
    Depends { name: "qbs-setup-toolchains" }

    Group {
        name: "testdata"
        prefix: "testdata-clangdb/"
        files: ["**/*"]
        fileTags: []
    }

    files: [
        "../shared.h",
        "tst_blackboxbase.cpp",
        "tst_blackboxbase.h",
        "tst_clangdb.cpp",
        "tst_clangdb.h",
    ]
    // TODO: Use Utilities.cStringQuote
    cpp.defines: base.concat(['SRCDIR="' + path + '"'])
}
