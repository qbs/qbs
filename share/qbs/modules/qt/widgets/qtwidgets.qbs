import qbs.base 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    Depends { id: qtcore; name: "Qt.core" }
    Depends { name: "Qt.gui" }
    qtModuleName: qtcore.versionMajor >= 5 ? 'Widgets' : undefined
}
