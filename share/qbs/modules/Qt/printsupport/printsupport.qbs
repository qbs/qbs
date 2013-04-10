import qbs 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    qtModuleName: Qt.core.versionMajor >= 5 ? 'PrintSupport' : undefined
}
