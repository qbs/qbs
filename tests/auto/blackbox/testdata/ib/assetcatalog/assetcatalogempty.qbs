import qbs

Project {
    property bool includeIconset

    CppApplication {
        Depends { name: "ib" }
        files: {
            var filez = ["main.c", "empty.xcassets", "MainMenu.xib"];
            if (project.includeIconset)
                filez.push("empty.xcassets/empty.iconset");
            if (qbs.hostOSVersionMinor >= 10) // need OS X 10.10 to build SBs
                filez.push("Storyboard.storyboard");
            return filez;
        }
    }
}
