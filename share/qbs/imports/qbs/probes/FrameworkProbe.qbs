import qbs 1.0

PathProbe {
    platformPaths: [
        "~/Library/Frameworks",
        "/usr/local/lib",
        "/Library/Frameworks",
        "/System/Library/Frameworks"
    ]

    nameFilter: {
        return function(name) {
            return name + ".framework";
        }
    }
}
