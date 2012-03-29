import qbs.base 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    qtModuleName: "Quick"
    condition: qtcore.versionMajor === 5
    Depends { id: qtcore; name: "Qt.core" }
    Depends { name: "Qt.qml" }
    repository: "qtdeclarative"
}

