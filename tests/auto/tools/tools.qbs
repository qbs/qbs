import qbs
import qbs.Utilities

QbsAutotest {
    Depends { name: "qbsversion" }

    testName: "tools"
    condition: qbsbuildconfig.enableUnitTests
    files: [
        "tst_tools.cpp",
        "tst_tools.h"
    ]

    cpp.defines: base.concat(["QBS_VERSION=" + Utilities.cStringQuote(qbsversion.version)])
}
