import qbs.FileInfo
import qbs.Host

Project {
    WindowsInstallerPackage {
        name: "QbsSetup"
        targetName: "qbs"
        files: ["QbsSetup.wxs", "ExampleScript.bat"]
        wix.defines: ["scriptName=ExampleScript.bat"]
        wix.extensions: ["WixBalExtension", "WixUIExtension"]
        qbs.targetPlatform: "windows"

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
    }

    WindowsInstallerPackage {
        name: "RegressionBuster9000"
        files: ["QbsSetup.wxs", "Qt.wxs", "de.wxl"]
        wix.defines: ["scriptName=ExampleScript.bat"]
        wix.cultures: []
        qbs.architecture: original || "x86"
        qbs.targetPlatform: "windows"
    }
}
