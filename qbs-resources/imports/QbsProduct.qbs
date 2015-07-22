import qbs
import QbsFunctions

Product {
    Depends { name: "Qt.core" }
    property string minimumQtVersion: "5.1.0"
    cpp.defines: ["QT_NO_CAST_FROM_ASCII", "QT_NO_PROCESS_COMBINED_ARGUMENT_START"]
    condition: QbsFunctions.versionIsAtLeast(Qt.core.version, minimumQtVersion)
}
