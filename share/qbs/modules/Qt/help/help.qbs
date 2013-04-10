import qbs 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    qtModuleName: 'Help'
    repository: Qt.core.versionMajor === 5 ? 'qttools' : undefined
}

