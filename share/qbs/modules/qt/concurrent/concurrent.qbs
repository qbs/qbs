import qbs.base 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    qtModuleName: qt.core.versionMajor >= 5 ? 'Concurrent' : undefined
}

