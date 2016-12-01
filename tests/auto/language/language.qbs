import qbs

QbsAutotest {
    testName: "language"
    condition: qbsbuildconfig.enableUnitTests
    files: "tst_language.cpp"

    Group {
        name: "testdata"
        prefix: "testdata/"
        files: ["**/*"]
        fileTags: []
    }
}
