import qbs.Utilities

Project {
    condition: {
        var result = qbs.targetOS.contains("macos");
        if (!result)
            console.info("Skip this test");
        return result;
    }
    property bool includeIconset

    CppApplication {
        Depends { name: "ib" }
        files: {
            var filez = ["main.c", "MainMenu.xib"];
            if (project.includeIconset)
                filez.push("empty.xcassets/empty.iconset");
            else if (Utilities.versionCompare(xcode.version, "5") >= 0)
                filez.push("empty.xcassets");
            if ((qbs.hostOSVersionMajor >= 11
                 || qbs.hostOSVersionMinor >= 10) // need macOS 10.10 or higher to build SBs
                    && cpp.minimumMacosVersion !== undefined
                    && Utilities.versionCompare(cpp.minimumMacosVersion, "10.10") >= 0)
                filez.push("Storyboard.storyboard");
            return filez;
        }
    }
}
