import qbs.Utilities

Product {
    condition: {
        var ok = Utilities.versionCompare(Qt.core.version, "5.0.0") >= 0;
        if (!ok)
            console.info("Qt is too old");
        return ok;
    }
    name: "QDoc Test"
    type: ["qdoc-html", "qch"]

    Depends { name: "Qt.core" }

    files: ["qdoc.qdoc"]

    Group {
        name: "main qdocconf file"
        files: "qdoc.qdocconf"
        fileTags: "qdocconf-main"
    }
}
