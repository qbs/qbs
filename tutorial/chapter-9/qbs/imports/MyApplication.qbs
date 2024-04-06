CppApplication {
    Depends { name: "mybuildconfig" }
    version: mybuildconfig.productVersion

    cpp.rpaths: mybuildconfig.libRPaths
    consoleApplication: true
    installDir: mybuildconfig.appInstallDir
    install: true
    installDebugInformation: project.installDebugInformation
}
