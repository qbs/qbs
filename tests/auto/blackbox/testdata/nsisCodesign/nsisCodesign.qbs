Project {
    property bool enableSigning: false
    property string certificatePath
    property string certificatePassword
    property string hashAlgorithm
    property string signingTimestamp

    NSISSetup {
        property bool _test: {
            var present = qbs.targetOS.includes("windows") && nsis.present;
            console.info("has nsis: " + present);
        }

        name: "QbsSetup"
        targetName: "qbs.setup.test"

        Properties {
            condition: nsis.present
            nsis.defines: ["batchFile=hello.bat"]
        }

        files: ["hello.nsi", "hello.bat"]

        codesign.enableCodeSigning: project.enableSigning
        codesign.certificatePath: project.certificatePath
        codesign.certificatePassword: project.certificatePassword
        codesign.hashAlgorithm: project.hashAlgorithm
        codesign.signingTimestamp: project.signingTimestamp
        property bool dummy: {
            if (codesign.codesignPath)
                console.info("osslsigncode path: %%" + codesign.codesignPath + "%%");
        }
    }
}
