import qbs.base 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    Depends { name: "Qt.gui" }
    qtModuleName: qt.core.versionMajor >= 5 ? 'Widgets' : undefined
}
