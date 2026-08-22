import qbs.FileInfo
import qbs.Host

Project {
    property bool enableSigning: false
    property string hashAlgorithm
    property string subjectName
    property string signingTimestamp

    WindowsInstallerPackage {
        name: "QbsSetup"
        targetName: "qbs"
        files: ["QbsSetup.wxs", "ExampleScript.bat"]
        wix.defines: ["scriptName=ExampleScript.bat"]
        wix.extensions: ["WixBalExtension", "WixUIExtension"]
        qbs.targetPlatform: "windows"
        codesign.enableCodeSigning: project.enableSigning
        codesign.hashAlgorithm: project.hashAlgorithm
        codesign.subjectName: project.subjectName
        codesign.signingTimestamp: project.signingTimestamp
        codesign.timestampAlgorithm: "sha256"
        property bool dummy: {
            if (codesign.codesignPath)
                console.info("signtool path: %%" + codesign.codesignPath + "%%");
        }

        Export {
            Depends { name: "wix" }
            wix.defines: base.concat(["msiName=" +
                FileInfo.joinPaths(exportingProduct.buildDirectory,
                    exportingProduct.targetName + wix.windowsInstallerSuffix)])
        }
    }

    WindowsSetupPackage {
        Depends { name: "QbsSetup" }
        condition: Host.os().includes("windows") // currently does not work in Wine with WiX 3.9
        name: "QbsBootstrapper"
        targetName: "qbs-setup-" + qbs.architecture
        files: ["QbsBootstrapper.wxs"]
        qbs.architecture: original || "x86"
        qbs.targetPlatform: "windows"
        codesign.enableCodeSigning: project.enableSigning
        codesign.hashAlgorithm: project.hashAlgorithm
        codesign.subjectName: project.subjectName
        codesign.signingTimestamp: project.signingTimestamp
        codesign.timestampAlgorithm: "sha256"
    }

    WindowsInstallerPackage {
        name: "RegressionBuster9000"
        files: ["QbsSetup.wxs", "Qt.wxs", "de.wxl"]
        wix.defines: ["scriptName=ExampleScript.bat"]
        wix.cultures: []
        qbs.architecture: original || "x86"
        qbs.targetPlatform: "windows"
        codesign.enableCodeSigning: project.enableSigning
        codesign.hashAlgorithm: project.hashAlgorithm
        codesign.subjectName: project.subjectName
        codesign.signingTimestamp: project.signingTimestamp
        codesign.timestampAlgorithm: "sha256"
    }
}
