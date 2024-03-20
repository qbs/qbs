//! [0]
CppApplication {
    Depends { name: "mybuildconfig" }
    installDir: mybuildconfig.appInstallDir

    version: "1.0.0"
    // ...
//! [0]

    consoleApplication: true
    install: true
    installDebugInformation: project.installDebugInformation
}
