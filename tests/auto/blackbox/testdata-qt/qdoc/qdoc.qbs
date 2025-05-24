import qbs.Host
import qbs.Utilities

Product {
    condition: {
        if (Utilities.versionCompare(Qt.core.version, "5.0.0") < 0) {
            console.info("Qt is too old");
            return false;
        }
        if (qbs.targetPlatform !== Host.platform() || qbs.architecture !== Host.architecture()) {
            console.info("target platform/arch differ from host platform/arch");
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
