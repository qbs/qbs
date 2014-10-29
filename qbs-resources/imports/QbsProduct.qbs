import qbs
import QbsFunctions

Product {
    Depends { name: "Qt.core" }
    property string minimumQtVersion: "5.1.0"
    condition: QbsFunctions.versionIsAtLeast(Qt.core.version, minimumQtVersion)
}
