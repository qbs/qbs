import qbs 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    condition: qt.core.versionMajor >= 5
    Depends { name: "qt.webkit" }
    Depends { name: "qt.gui" }
    qtModuleName: 'WebKitWidgets'
    repository: 'qtwebkit'
}
