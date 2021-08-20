import qbs.FileInfo

QbsProduct {
    Depends { name: "qbscore" }
    Depends { name: "qbsversion" }
    Depends { name: "qbsconsolelogger" }
    type: ["application", "qbsapplication"]
    version: qbsversion.version
    consoleApplication: true
    Group {
        fileTagsFilter: product.type
            .concat(qbs.debugInformation ? ["debuginfo_app"] : [])
        qbs.install: true
        qbs.installDir: targetInstallDir
        qbs.installSourceBase: buildDirectory
    }
    targetInstallDir: qbsbuildconfig.appInstallDir
}
