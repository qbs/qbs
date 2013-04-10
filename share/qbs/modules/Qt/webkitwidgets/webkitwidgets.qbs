import qbs 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    condition: Qt.core.versionMajor >= 5
    Depends { name: "Qt.webkit" }
    Depends { name: "Qt.gui" }
    qtModuleName: 'WebKitWidgets'
    repository: 'qtwebkit'
}
