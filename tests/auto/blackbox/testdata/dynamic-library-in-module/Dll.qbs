DynamicLibrary {
    Depends { name: "cpp" }
    Depends { name: "bundle"; condition: qbs.targetOS.includes("darwin") }
    Properties {
        condition: qbs.targetOS.includes("darwin")
        bundle.isBundle: false
        cpp.minimumMacosVersion: "10.7" // For -rpath
    }

    install: true
    installImportLib: true
    qbs.installPrefix: ""
    installDir: ""
    importLibInstallDir: ""
}
