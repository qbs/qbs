import qbs.base 1.0

GenericGCC {
    condition: false
    staticLibraryPrefix: "lib"
    dynamicLibraryPrefix: "lib"
    executablePrefix: ""
    staticLibrarySuffix: ".a"
    dynamicLibrarySuffix: ".so"
    executableSuffix: ""
}

