import "../autotest.qbs" as AutoTest

AutoTest {
    testName: "api"
    files: ["tst_api.h", "tst_api.cpp"]
    cpp.defines: base.concat(['SRCDIR="' + path + '"'])
}
