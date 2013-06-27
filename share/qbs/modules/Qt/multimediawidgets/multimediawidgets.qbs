import qbs 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    qtModuleName: 'MultimediaWidgets'
    repository: Qt.core.versionMajor === 5 ? 'qtmultimedia' : undefined
}

