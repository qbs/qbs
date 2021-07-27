import qbs.FileInfo
import qbs.Utilities

QbsProduct {
    type: ["application", "autotest"]
    consoleApplication: true
    property string testName
    name: "tst_" + testName
    property string targetInstallDir: qbsbuildconfig.appInstallDir
    Depends { name: "Qt.testlib" }
    Depends { name: "qbscore" }
    Depends { name: "qbsbuildconfig" }
    cpp.defines: [ // deliberately override base defines
        "QBS_TEST_SUITE_NAME=" + Utilities.cStringQuote(testName.toUpperCase().replace("-", "_"))
    ]
    cpp.includePaths: [
        "../../../src",
    ]

    qbs.commonRunEnvironment: ({
        "QBS_INSTALL_DIR": FileInfo.joinPaths(qbs.installRoot, qbs.installPrefix)
    })
    Group {
        fileTagsFilter: product.type
        qbs.install: true
        qbs.installDir: targetInstallDir
    }
}
