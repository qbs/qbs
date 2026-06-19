Project {
    property bool enableSigning: false
    property string hashAlgorithm
    property string subjectName
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
