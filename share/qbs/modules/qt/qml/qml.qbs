import qbs.base 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    qtModuleName: "qml"
    condition: qtcore.versionMajor === 5
    Depends { id: qtcore; name: "qt.core" }
    Depends {
        name: "Qt"
        submodules: ["widgets", "script"]
    }
    repository: 'qtdeclarative'
}

