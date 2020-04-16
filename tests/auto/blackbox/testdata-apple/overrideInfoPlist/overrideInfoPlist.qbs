CppApplication {
    Depends { name: "bundle" }
    cpp.minimumMacosVersion: "10.7"
    files: ["main.c", "Override-Info.plist"]

    Properties {
        condition: qbs.targetOS.contains("darwin")
        bundle.isBundle: true
        bundle.identifierPrefix: "com.test"

        bundle.infoPlist: ({
            "CFBundleName": "My Bundle",
            "OverriddenValue": "The overridden value",
        })
    }
}
