import qbs
import qbs.Utilities

QbsAutotest {
    testName: "api"
    files: ["../shared.h", "tst_api.h", "tst_api.cpp"]
    cpp.defines: base.concat([
        "SRCDIR=" + Utilities.cStringQuote(path),
        "QBS_RELATIVE_LIBEXEC_PATH=" + Utilities.cStringQuote(qbsbuildconfig.relativeLibexecPath),
        "QBS_RELATIVE_SEARCH_PATH=" + Utilities.cStringQuote(qbsbuildconfig.relativeSearchPath),
        "QBS_RELATIVE_PLUGINS_PATH=" + Utilities.cStringQuote(qbsbuildconfig.relativePluginsPath)
    ]).concat(qbsbuildconfig.enableProjectFileUpdates ? ["QBS_ENABLE_PROJECT_FILE_UPDATES"] : [])

    Group {
        name: "testdata"
        prefix: "testdata/"
        files: ["**/*"]
        fileTags: []
    }
}
