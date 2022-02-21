import qbs.FileInfo
import qbs.Utilities

QbsAutotest {
    Depends {
        name: "Qt.core5compat";
        condition: Utilities.versionCompare(Qt.core.version, "6.0.0") >= 0
    }
    Depends { name: "quickjs"; cpp.link: false }
}
