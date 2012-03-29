import qbs.base 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    qtModuleName: "Declarative"
    condition: qtcore.versionMajor === 4
    Depends { id: qtcore; name: "Qt.core" }
    Depends {
        name: "Qt"
        submodules: ["gui", "network", "script", "xmlpatterns"]
    }
}

