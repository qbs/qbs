import qbs.base 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    qtModuleName: "Declarative"
    Depends {
        name: "Qt"
        submodules: ["gui", "network", "script", "xmlpatterns"]
    }
}

