import qbs 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    qtModuleName: @name@
    Depends { name: "Qt"; submodules: @dependencies@}
    hasLibrary: @has_library@
    cpp.includePaths: @includes@
    @special_properties@
}
