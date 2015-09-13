import qbs

Project {
    property stringList sdks: []

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
            console.info("Actual SDK list: " + project.sdks.join(", "));

            var msg = "Unexpected SDK list [" + xcode.availableSdkVersions.join(", ") + "]";
            var testArraysEqual = function(a, b) {
                if (!a || !b || a.length !== b.length) {
                    throw msg;
                }

                for (var i = 0; i < a.length; ++i) {
                    if (a[i] !== b[i]) {
                        throw msg;
                    }
                }
            }

            testArraysEqual(project.sdks, xcode.availableSdkVersions);
        }
    }
}
