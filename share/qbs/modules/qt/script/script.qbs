import qbs.base 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    qtModuleName: 'Script'
    repository: qtcore.versionMajor === 5 ? 'qtscript' : undefined
}

