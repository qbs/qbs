import qbs

CppApplication {
    condition: qbs.targetOS.contains("darwin")
    type: ["application"]
    files: ["main.m"]
    cpp.frameworks: ["Foundation"]
    cpp.infoPlist: {
        return {
            "QBS": "org.qt-project.qbs.testdata.embedInfoPlist"
        };
    }
}
