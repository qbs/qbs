import qbs.base 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    qtModuleName: "Declarative"
    condition: qt.core.versionMajor === 4
    Depends {
        name: "Qt"
        submodules: ["gui", "network", "script", "xmlpatterns"]
    }
}

