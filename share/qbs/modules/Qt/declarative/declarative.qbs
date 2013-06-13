import qbs 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    qtModuleName: "Declarative"

    property bool qmlDebugging: false

    Depends {
        name: "Qt"
        submodules: ["gui", "network", "script", "xmlpatterns"]
    }

    cpp.defines: qmlDebugging ? base.concat("QT_DECLARATIVE_DEBUG") : base
}

