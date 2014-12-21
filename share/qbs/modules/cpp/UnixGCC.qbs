import qbs 1.0

GenericGCC {
    condition: false
    staticLibraryPrefix: "lib"
    dynamicLibraryPrefix: "lib"
    loadableModulePrefix: "lib"
    executablePrefix: ""
    staticLibrarySuffix: ".a"
    dynamicLibrarySuffix: ".so"
    loadableModuleSuffix: ""
    executableSuffix: ""
    debugInfoSuffix: ".debug"
}

