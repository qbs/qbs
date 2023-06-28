QtModule {
    qtModuleName: @name@
    Depends { name: "Qt"; submodules: @dependencies@}

    architectures: @archs@
    targetPlatform: @targetPlatform@
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
    pluginTypes: @pluginTypes@
    moduleConfig: @moduleConfig@
    cpp.defines: @defines@
    cpp.systemIncludePaths: @includes@
    cpp.libraryPaths: @libraryPaths@
    @additionalContent@
}
