import qbs.base 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    Depends { id: qtcore; name: "Qt.core" }
    Depends { name: "Qt.gui" }
    condition: qtcore.versionMajor >= 5
    qtModuleName: 'Widgets'
}
