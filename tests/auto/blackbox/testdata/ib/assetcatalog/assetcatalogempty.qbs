import qbs

Project {
    property bool includeIconset

    CppApplication {
        Depends { name: "ib" }
        files: {
            var filez = ["main.c", "empty.xcassets"];
            if (project.includeIconset)
                filez.push("empty.xcassets/empty.iconset");
            return filez;
        }
    }
}
