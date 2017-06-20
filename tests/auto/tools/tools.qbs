import qbs

QbsAutotest {
    Depends { name: "qbsversion" }

    testName: "tools"
    condition: qbsbuildconfig.enableUnitTests
    files: [
        "tst_tools.cpp",
        "tst_tools.h"
    ]

    // TODO: Use Utilities.cStringQuote
    cpp.defines: ['QBS_VERSION="' + qbsversion.version + '"']
}
