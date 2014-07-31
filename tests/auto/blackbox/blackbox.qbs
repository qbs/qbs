import "../autotest.qbs" as AutoTest

AutoTest {
    testName: "blackbox"
    files: ["../shared.h", "tst_blackbox.h", "tst_blackbox.cpp", ]
    cpp.defines: base.concat(['SRCDIR="' + path + '"'])
}
