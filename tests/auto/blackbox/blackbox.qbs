import qbs 1.0

QbsAutotest {
    testName: "blackbox"
    files: ["../shared.h", "tst_blackbox.h", "tst_blackbox.cpp", ]
    cpp.defines: base.concat(['SRCDIR="' + path + '"'])
}
