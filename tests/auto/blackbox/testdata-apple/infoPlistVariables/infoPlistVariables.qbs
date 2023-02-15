CppApplication {
    Depends { name: "bundle" }
    cpp.minimumMacosVersion: "10.7"
    files: ["main.c", "Info.plist"]

    Properties {
        condition: qbs.targetOS.includes("darwin")
        bundle.isBundle: true
        bundle.identifierPrefix: "com.test"
        bundle.extraEnv: {
            var result = original;
            result["EXE"] = "EXECUTABLE";
            return result;
        }
    }
}
