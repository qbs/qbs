import qbs

Project {
    WindowsInstallerPackage {
        name: "QbsSetup"
        targetName: "qbs-" + qbs.architecture
        files: ["QbsSetup.wxs", "ExampleScript.bat"]
        wix.defines: ["scriptName=ExampleScript.bat"]
    }

    WindowsSetupPackage {
        Depends { name: "QbsSetup" }
        name: "QbsBootstrapper"
        targetName: "qbs-setup-" + qbs.architecture
        files: ["QbsBootstrapper.wxs"]
        wix.defines: ["msiName=qbs-" + qbs.architecture + ".msi"]
    }

    WindowsInstallerPackage {
        name: "RegressionBuster9000"
        files: ["QbsSetup.wxs", "Qt.wxs"]
        wix.defines: ["scriptName=ExampleScript.bat"]
        wix.cultures: []
    }
}
