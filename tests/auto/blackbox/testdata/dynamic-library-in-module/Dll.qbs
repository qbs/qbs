DynamicLibrary {
    Depends { name: "cpp" }
    Depends { name: "bundle"; condition: qbs.targetOS.contains("darwin") }
    Properties {
        condition: qbs.targetOS.contains("darwin")
        bundle.isBundle: false
        cpp.minimumMacosVersion: "10.7" // For -rpath
    }

    install: true
    installImportLib: true
    qbs.installPrefix: ""
    installDir: ""
    importLibInstallDir: ""
}
