import qbs
import qbs.FileInfo

Project {
    WindowsInstallerPackage {
        name: "QbsSetup"
        targetName: "qbs-" + qbs.architecture
        files: ["QbsSetup.wxs", "ExampleScript.bat"]
        wix.defines: ["scriptName=ExampleScript.bat"]

        Export {
            Depends { name: "wix" }
            wix.defines: base.concat(["msiName=" +
                FileInfo.joinPaths(product.buildDirectory,
                    targetName + wix.windowsInstallerSuffix)])
        }
    }

    WindowsSetupPackage {
        Depends { name: "QbsSetup" }
        name: "QbsBootstrapper"
        targetName: "qbs-setup-" + qbs.architecture
        files: ["QbsBootstrapper.wxs"]
    }

    WindowsInstallerPackage {
        name: "RegressionBuster9000"
        files: ["QbsSetup.wxs", "Qt.wxs", "de.wxl"]
        wix.defines: ["scriptName=ExampleScript.bat"]
        wix.cultures: []
    }
}
