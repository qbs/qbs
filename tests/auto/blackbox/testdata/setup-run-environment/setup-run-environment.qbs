import qbs.FileInfo

Project {
    DynamicLibrary { // Product dependency, installed
        name: "lib1"
        Depends { name: "cpp" }

        files: ["lib1.cpp"]

        install: !qbs.targetOS.contains("darwin")
        installImportLib: true
        installDir: "lib1"
        importLibInstallDir: installDir

        Group {
        condition: qbs.targetOS.contains("darwin")
            fileTagsFilter: ["bundle.content"]
            qbs.install: true
            qbs.installSourceBase: destinationDirectory
        }
    }

    DynamicLibrary { // Product dependency, non-installed
        name: "lib2"
        Depends { name: "cpp" }
        Depends { name: "lib5" }

    Properties {
            condition: qbs.targetOS.contains("darwin")
            bundle.isBundle: false
        }

        files: ["lib2.cpp"]
    }

    DynamicLibrary { // Non-dependency, referred to by full path
        name: "lib3"
        Depends { name: "cpp" }

        files: ["lib3.cpp"]

        Properties {
            condition: qbs.targetOS.contains("darwin")
            bundle.isBundle: false
        }

        install: true
        installImportLib: true
        installDir: "lib3"
        importLibInstallDir: installDir
    }

    DynamicLibrary { // Non-dependency, referred to by name
        name: "lib4"
        Depends { name: "cpp" }

        files: ["lib4.cpp"]

        Properties {
            condition: qbs.targetOS.contains("darwin")
            bundle.isBundle: false
        }

        // Testing shows that clang (8.0) does not find dynamic libraries via
        // the -L<dir> and -l<libname> mechanism unless the name is "lib<libname>.a".
        Properties {
            condition: qbs.hostOS.contains("windows") && qbs.toolchain.contains("clang")
            cpp.dynamicLibraryPrefix: "lib"
            cpp.dynamicLibraryImportSuffix: ".a"
        }
        cpp.dynamicLibraryPrefix: original
        cpp.dynamicLibraryImportSuffix: original

        install: true
        installImportLib: true
        installDir: "lib4"
        importLibInstallDir: installDir
    }

    DynamicLibrary { // Recursive product dependency
        name: "lib5"
        Depends { name: "cpp" }

        Properties {
            condition: qbs.targetOS.contains("darwin")
            bundle.isBundle: false
        }

        files: ["lib5.cpp"]
    }

    CppApplication {
        condition: {
            var result = qbs.targetPlatform === qbs.hostPlatform;
            if (!result)
                console.info("targetPlatform differs from hostPlatform");
            return result;
        }
        name: "app"
        consoleApplication: true
        files: "main.cpp"

        Depends { name: "lib1" }
        Depends { name: "lib2" }
        Depends { name: "lib3"; cpp.link: false }
        Depends { name: "lib4"; cpp.link: false }

        property string fullInstallPrefix: FileInfo.joinPaths(qbs.installRoot, qbs.installPrefix)
        property string lib3FilePath: FileInfo.joinPaths(fullInstallPrefix, "lib3",
                cpp.dynamicLibraryPrefix + "lib3" + (qbs.targetOS.contains("windows")
                                                     ? cpp.dynamicLibraryImportSuffix
                                                     : cpp.dynamicLibrarySuffix))
        cpp.dynamicLibraries: [lib3FilePath, "lib4"]
        cpp.libraryPaths: FileInfo.joinPaths(fullInstallPrefix, "lib4")
    }

    Probe {
        id: osPrinter
        property bool isWindows: qbs.targetOS.contains("windows")
        configure: {
            console.info("is windows");
            found = true;
        }
    }
}
