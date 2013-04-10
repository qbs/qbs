import qbs 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    qtModuleName: 'Script'
    repository: Qt.core.versionMajor === 5 ? 'qtscript' : undefined
}

