import qbs.base 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    Depends { name: "qt.gui"; condition: qt.core.versionMajor === 4}
    qtModuleName: 'WebKit'
    repository: qt.core.versionMajor === 5 ? 'qtwebkit' : undefined
}

