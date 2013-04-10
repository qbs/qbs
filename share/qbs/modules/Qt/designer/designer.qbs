import qbs 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    qtModuleName: 'Designer'
    repository: Qt.core.versionMajor >= 5 ? 'qttools' : undefined
}

