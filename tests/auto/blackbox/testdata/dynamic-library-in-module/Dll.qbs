import qbs

DynamicLibrary {
    Depends { name: "cpp" }
    Depends { name: "bundle"; condition: qbs.targetOS.contains("darwin") }
    Properties {
        condition: qbs.targetOS.contains("darwin")
        bundle.isBundle: false
        cpp.minimumMacosVersion: "10.5" // For -rpath
    }

    install: true
    installImportLib: true
    qbs.installPrefix: ""
    installDir: ""
    importLibInstallDir: ""
}
