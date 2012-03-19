import qbs.base 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    Depends { id: qtcore; name: "Qt.core" }
    Depends { name: "qt.gui"; condition: qtcore.versionMajor === 4}
    qtModuleName: 'WebKit'
    repository: qtcore.versionMajor === 5 ? 'qtwebkit' : undefined
}

