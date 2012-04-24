import qbs.base 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    qtModuleName: "qml"
    condition: qt.core.versionMajor === 5
    Depends {
        name: "Qt"
        submodules: ["widgets", "script"]
    }
    repository: 'qtdeclarative'
}

