import qbs 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    Depends { name: "Qt.gui" }
    qtModuleName: Qt.core.versionMajor >= 5 ? 'Widgets' : undefined
}
