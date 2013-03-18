import "../autotest.qbs" as AutoTest

AutoTest {
    testName: "blackbox"
    files: ["tst_blackbox.h", "tst_blackbox.cpp"]
    cpp.defines: base.concat(['SRCDIR="' + path + '"'])
}
