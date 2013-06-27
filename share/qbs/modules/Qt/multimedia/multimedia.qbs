import qbs 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    qtModuleName: 'Multimedia'
    repository: Qt.core.versionMajor === 5 ? 'qtmultimedia' : undefined
}

