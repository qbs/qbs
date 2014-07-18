import "../autotest.qbs" as AutoTest

AutoTest {
    testName: "api"
    files: ["tst_api.h", "tst_api.cpp"]
    cpp.defines: base
        .concat(['SRCDIR="' + path + '"'])
        .concat(project.enableProjectFileUpdates ? ["QBS_ENABLE_PROJECT_FILE_UPDATES"] : [])
}
