CppApplication {
    Depends { name: "mybuildconfig" }
    version: "1.0.0"

    cpp.rpaths: mybuildconfig.libRPaths
    consoleApplication: true
    installDir: mybuildconfig.appInstallDir
    install: true
    installDebugInformation: project.installDebugInformation
}
