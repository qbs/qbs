import qbs 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    condition: true
    qtModuleName: "JsonStream"
    internalQtModuleName: "QtAddOnJsonStream"
    Depends { name: "Qt.network" }
}
