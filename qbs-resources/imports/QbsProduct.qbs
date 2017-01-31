import qbs
import QbsFunctions

Product {
    Depends { name: "qbsbuildconfig" }
    Depends { name: "Qt.core" }
    property string minimumQtVersion: "5.6.0"
    property bool install: true
    cpp.defines: {
        var res = ["QT_NO_CAST_FROM_ASCII", "QT_NO_PROCESS_COMBINED_ARGUMENT_START"];
        if (qbs.toolchain.contains("msvc"))
            res.push("_SCL_SECURE_NO_WARNINGS");
        if (qbs.enableDebugCode)
            res.push("QT_STRICT_ITERATORS");
        return res;
    }
    cpp.enableExceptions: true
    condition: QbsFunctions.versionIsAtLeast(Qt.core.version, minimumQtVersion)
}
