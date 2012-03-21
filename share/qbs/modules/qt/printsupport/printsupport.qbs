import qbs.base 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    Depends { id: qtcore; name: "Qt.core" }
    qtModuleName: qtcore.versionMajor >= 5 ? 'PrintSupport' : undefined
}
