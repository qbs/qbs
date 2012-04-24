import qbs.base 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    qtModuleName: 'Designer'
    repository: qt.core.versionMajor >= 5 ? 'qttools' : undefined
}

