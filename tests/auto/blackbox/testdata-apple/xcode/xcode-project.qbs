Project {
    property var sdks: {}

    Product {
        Depends { name: "xcode" }

        consoleApplication: {
            console.info("Developer directory: " + xcode.developerPath);
            console.info("SDK: " + xcode.sdk);
            console.info("Target devices: " + xcode.targetDevices.join(", "));
            console.info("SDK name: " + xcode.sdkName);
            console.info("SDK version: " + xcode.sdkVersion);
            console.info("Latest SDK name: " + xcode.latestSdkName);
            console.info("Latest SDK version: " + xcode.latestSdkVersion);
            console.info("Available SDK names: " + xcode.availableSdkNames.join(", "));
            console.info("Available SDK versions: " + xcode.availableSdkVersions.join(", "));

            var targetOsToKey = function(targetOS) {
                if (targetOS.includes("ios"))
                    return "iphoneos";
                if (targetOS.includes("ios-simulator"))
                    return "iphonesimulator";
                if (targetOS.includes("macos"))
                    return "macosx";
                if (targetOS.includes("tvos"))
                    return "appletvos";
                if (targetOS.includes("tvos-simulator"))
                    return "appletvsimulator";
                if (targetOS.includes("watchos"))
                    return "watchos";
                if (targetOS.includes("watchos-simulator"))
                    return "watchossimulator";
                throw "Unsupported OS" + targetOS;
            }

            var actualList = project.sdks[targetOsToKey(qbs.targetOS)];
            console.info("Actual SDK list: " + actualList.join(", "));

            var msg = "Unexpected SDK list [" + xcode.availableSdkVersions.join(", ") + "]";
            var testArraysEqual = function(a, b) {
                if (!a || !b || a.length !== b.length) {
                    throw msg;
                }

                for (var i = 0; i < a.length; ++i) {
                    var version1 = a[i].split('.');
                    var version2 = b[i].split('.');
                    for (var j = 0; j < version1.length; ++j) {
                        if (version1[j] !== version2[j])
                            throw msg;
                    }
                }
            }

            testArraysEqual(actualList, xcode.availableSdkVersions);
        }
    }
}
