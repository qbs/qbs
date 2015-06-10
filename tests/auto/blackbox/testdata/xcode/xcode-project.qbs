import qbs

Project {
    property stringList sdks: []

    Product {
        Depends { name: "xcode" }

        consoleApplication: {
            print("Developer directory: " + xcode.developerPath);
            print("SDK: " + xcode.sdk);
            print("Target devices: " + xcode.targetDevices.join(", "));
            print("SDK name: " + xcode.sdkName);
            print("SDK version: " + xcode.sdkVersion);
            print("Latest SDK name: " + xcode.latestSdkName);
            print("Latest SDK version: " + xcode.latestSdkVersion);
            print("Available SDK names: " + xcode.availableSdkNames.join(", "));
            print("Available SDK versions: " + xcode.availableSdkVersions.join(", "));
            print("Actual SDK list: " + project.sdks.join(", "));

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
