import qbs

QbsAutotest {
    testName: "api"
    files: ["../shared.h", "tst_api.h", "tst_api.cpp"]
    // TODO: Use Utilities.cStringQuote
    cpp.defines: base.concat([
        'SRCDIR="' + path + '"',
        'QBS_RELATIVE_LIBEXEC_PATH="' + qbsbuildconfig.relativeLibexecPath + '"',
        'QBS_RELATIVE_SEARCH_PATH="' + qbsbuildconfig.relativeSearchPath + '"',
        'QBS_RELATIVE_PLUGINS_PATH="' + qbsbuildconfig.relativePluginsPath + '"'
    ]).concat(qbsbuildconfig.enableProjectFileUpdates ? ["QBS_ENABLE_PROJECT_FILE_UPDATES"] : [])

    Group {
        name: "testdata"
        prefix: "testdata/"
        files: ["**/*"]
        fileTags: []
    }
}
