import qbs 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    qtModuleName: "Quick"
    condition: Qt.core.versionMajor === 5

    property bool qmlDebugging: false

    Depends { name: "Qt.qml" }
    repository: "qtdeclarative"

    cpp.defines: qmlDebugging ? base.concat("QT_QML_DEBUG") : base
}

