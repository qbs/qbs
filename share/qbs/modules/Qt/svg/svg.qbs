import qbs 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    qtModuleName: 'Svg'
    repository: Qt.core.versionMajor === 5 ? 'qtsvg' : undefined
}

