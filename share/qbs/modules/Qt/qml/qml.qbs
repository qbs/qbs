import qbs 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    qtModuleName: "Qml"
    condition: Qt.core.versionMajor === 5
    Depends {
        name: "Qt"
        submodules: ["widgets", "script"]
    }
    repository: 'qtdeclarative'
}

