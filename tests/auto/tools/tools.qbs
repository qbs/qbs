import qbs

QbsAutotest {
    testName: "tools"
    condition: qbsbuildconfig.enableUnitTests
    files: ["tst_tools.cpp"]
}
