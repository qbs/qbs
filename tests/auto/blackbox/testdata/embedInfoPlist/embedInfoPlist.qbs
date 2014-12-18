import qbs

CppApplication {
    condition: qbs.targetOS.contains("darwin")
    bundle.isBundle: false
    files: ["main.m"]
    cpp.frameworks: ["Foundation"]
    bundle.infoPlist: {
        return {
            "QBS": "org.qt-project.qbs.testdata.embedInfoPlist"
        };
    }
}
