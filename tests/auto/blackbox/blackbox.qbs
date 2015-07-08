import qbs 1.0

QbsAutotest {
    testName: "blackbox"
    Depends { name: "qbs_app" }
    Depends { name: "qbs-setup-toolchains" }
    files: ["../shared.h", "tst_blackbox.h", "tst_blackbox.cpp", ]
    cpp.defines: base.concat(['SRCDIR="' + path + '"'])

    Group {
        name: "testdata"
        prefix: "testdata/"
        files: ["**/*"]
        fileTags: []
    }
}
