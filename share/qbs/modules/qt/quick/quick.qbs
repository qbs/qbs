import qbs 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    qtModuleName: "Quick"
    condition: qt.core.versionMajor === 5
    Depends { name: "Qt.qml" }
    repository: "qtdeclarative"
}

