import qbs.base 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    qtModuleName: qtcore.versionMajor === 5 ? "Quick1" : undefined
    Depends { id: qtcore; name: "Qt.core" }
    Depends {
        condition: qtcore.versionMajor === 4
        name: "Qt"
        submodules: ["declarative"]
    }
    Depends {
        condition: qtcore.versionMajor === 5
        name: "Qt"
        submodules: ["gui", "network", "script", "xmlpatterns"]
    }
    repository: qtcore.versionMajor === 5 ? "qtquick1" : undefined
}

