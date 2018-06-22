import qbs.FileInfo

Project {
    InnoSetup {
        name: "QbsSetup"
        targetName: "qbs.setup.test"
        version: "1.5"
        files: [
            "test.iss"
        ]
        innosetup.verboseOutput: true
        innosetup.includePaths: ["inc"]
        innosetup.defines: ["MyProgram=" + name, "MyProgramVersion=" + version]
        innosetup.compilerFlags: ["/V9"]
        qbs.targetPlatform: "windows"
    }
    InnoSetup {
        name: "Example1"
        files: [FileInfo.joinPaths(innosetup.toolchainInstallPath, "Examples", name + ".iss")]
        qbs.targetPlatform: "windows"
    }
}
