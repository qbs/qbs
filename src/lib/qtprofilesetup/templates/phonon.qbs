import qbs 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    qtModuleName: "phonon"
    Depends { name: "Qt"; submodules: ['core'] }
    cpp.defines: @defines@
    cpp.includePaths: @includes@
}
