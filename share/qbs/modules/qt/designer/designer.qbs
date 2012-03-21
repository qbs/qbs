import qbs.base 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    qtModuleName: 'Designer'
    repository: qtcore.versionMajor >= 5 ? 'qttools' : undefined
}

