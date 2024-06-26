import qbs.Host
import qbs.Utilities

Product {
    condition: {
        if (Utilities.versionCompare(Qt.core.version, "5.0.0") < 0) {
            console.info("Qt is too old");
            return false;
        }
        if (qbs.targetPlatform !== Host.platform()) {
            console.info("targetPlatform differs from hostPlatform");
            return false;
        }
        return true;
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
