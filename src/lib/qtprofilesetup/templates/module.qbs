import qbs 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    qtModuleName: @name@
    Depends { name: "Qt"; submodules: @dependencies@}

    architecture: @arch@
    hasLibrary: @has_library@
    staticLibsDebug: @staticLibsDebug@
    staticLibsRelease: @staticLibsRelease@
    dynamicLibsDebug: @dynamicLibsDebug@
    dynamicLibsRelease: @dynamicLibsRelease@
    linkerFlagsDebug: @linkerFlagsDebug@
    linkerFlagsRelease: @linkerFlagsRelease@
    frameworksDebug: @frameworksDebug@
    frameworksRelease: @frameworksRelease@
    frameworkPathsDebug: @frameworkPathsDebug@
    frameworkPathsRelease: @frameworkPathsRelease@
    libNameForLinkerDebug: @libNameForLinkerDebug@
    libNameForLinkerRelease: @libNameForLinkerRelease@
    libFilePathDebug: @libFilePathDebug@
    libFilePathRelease: @libFilePathRelease@
    cpp.defines: @defines@
    cpp.includePaths: @includes@
    cpp.libraryPaths: @libraryPaths@
    @special_properties@
}
