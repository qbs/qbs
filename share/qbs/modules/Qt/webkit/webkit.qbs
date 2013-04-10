import qbs 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    Depends { name: "Qt.gui"; condition: Qt.core.versionMajor === 4}
    qtModuleName: 'WebKit'
    repository: Qt.core.versionMajor === 5 ? 'qtwebkit' : undefined
}

