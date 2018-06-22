import qbs.Utilities

Product {
    name: {
        var e = "unexpected comparison result";
        if (Utilities.versionCompare("1.5", "1.5") !== 0)
            throw e;
        if (Utilities.versionCompare("1.5", "1.5.0") !== 0)
            throw e;
        if (Utilities.versionCompare("1.5", "1.6") >= 0)
            throw e;
        if (Utilities.versionCompare("1.6", "1.5") <= 0)
            throw e;
        if (Utilities.versionCompare("2.5", "1.6") <= 0)
            throw e;
        if (Utilities.versionCompare("1.6", "2.5") >= 0)
            throw e;
        return "versionCompare";
    }
}
