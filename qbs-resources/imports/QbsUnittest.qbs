import qbs.FileInfo
import qbs.Utilities

QbsAutotest {
    Depends {
        name: "Qt.core5compat";
        condition: Utilities.versionCompare(Qt.core.version, "6.0.0") >= 0
    }
    Depends {
        name: "Qt.script"
        condition: !qbsbuildconfig.useBundledQtScript
        required: false
    }
    Depends {
        name: "qbsscriptengine"
        condition: qbsbuildconfig.useBundledQtScript || !Qt.script.present
    }
    property stringList bundledQtScriptIncludes: qbsbuildconfig.useBundledQtScript
            || !Qt.script.present ? qbsscriptengine.includePaths : []
    cpp.includePaths: base.concat(bundledQtScriptIncludes)
}
