import qbs 1.0

QbsAutotest {
    testName: "blackbox"
    Depends { name: "qbs_app" }
    files: ["../shared.h", "tst_blackbox.h", "tst_blackbox.cpp", ]
    cpp.defines: base.concat(['SRCDIR="' + path + '"'])
}
