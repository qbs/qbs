import qbs.base 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    qtModuleName: 'Svg'
    repository: qt.core.versionMajor === 5 ? 'qtsvg' : undefined
}

