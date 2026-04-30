import qbs.FileInfo

Project {
    property bool enableSigning: false
    property string hashAlgorithm
    property string subjectName
    property string signingTimestamp

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
        codesign.enableCodeSigning: project.enableSigning
        codesign.hashAlgorithm: project.hashAlgorithm
        codesign.subjectName: project.subjectName
        codesign.signingTimestamp: project.signingTimestamp
        codesign.timestampAlgorithm: "sha256"
        property bool dummy: {
            if (codesign.codesignPath)
                console.info("signtool path: %%" + codesign.codesignPath + "%%");
        }
    }
    InnoSetup {
        name: "Example1"
        files: [FileInfo.joinPaths(innosetup.toolchainInstallPath, "Examples", name + ".iss")]
        codesign.enableCodeSigning: project.enableSigning
        codesign.hashAlgorithm: project.hashAlgorithm
        codesign.subjectName: project.subjectName
        codesign.signingTimestamp: project.signingTimestamp
        codesign.timestampAlgorithm: "sha256"
        property bool dummy: {
            if (codesign.codesignPath)
                console.info("signtool path: %%" + codesign.codesignPath + "%%");
        }
    }
}
