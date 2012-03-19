import qbs.base 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    qtModuleName: "Declarative"
    Depends { id: qtcore; name: "Qt.core" }
    Depends { name: "Qt.gui" }
    Depends { name: "qt.widgets"; condition: qtcore.versionMajor === 5 }
    Depends { name: "qt.script"; condition: qtcore.versionMajor === 5 }
    internalQtModuleName: qtcore.versionMajor === 5 ? "QtQuick1" : 'Qt' + qtModuleName
    repository: qtcore.versionMajor === 5 ? 'qtquick1' : undefined
}

