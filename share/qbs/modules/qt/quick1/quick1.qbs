import qbs.base 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    qtModuleName: qt.core.versionMajor === 5 ? "Quick1" : undefined
    Depends {
        condition: qt.core.versionMajor === 4
        name: "Qt"
        submodules: ["declarative"]
    }
    Depends {
        condition: qt.core.versionMajor === 5
        name: "Qt"
        submodules: ["gui", "network", "script", "xmlpatterns"]
    }
    repository: qt.core.versionMajor === 5 ? "qtquick1" : undefined
}

