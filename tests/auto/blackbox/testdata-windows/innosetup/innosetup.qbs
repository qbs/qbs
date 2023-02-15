import qbs.FileInfo

Project {
    InnoSetup {
        property bool _test: {
            var present = qbs.targetOS.includes("windows") && innosetup.present;
            console.info("has innosetup: " + present);
        }

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
    }
    InnoSetup {
        name: "Example1"
        files: [FileInfo.joinPaths(innosetup.toolchainInstallPath, "Examples", name + ".iss")]
    }
}
