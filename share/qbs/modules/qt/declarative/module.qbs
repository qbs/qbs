import qbs.base 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    qtModuleName: "Declarative"
    Depends { id: qtcore; name: "Qt.core" }
    Depends { name: "Qt.gui" }
    internalQtModuleName: qtcore.versionMajor === 5 ? "QtQuick1" : 'Qt' + qtModuleName
    repository: 'qtquick1'
}

