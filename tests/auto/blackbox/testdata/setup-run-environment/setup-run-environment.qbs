import qbs
import qbs.FileInfo

Project {
    DynamicLibrary { // Product dependency, installed
        name: "lib1"
        Depends { name: "cpp" }

        files: ["lib1.cpp"]

        Group {
        condition: !qbs.targetOS.contains("darwin")
            fileTagsFilter: ["dynamiclibrary", "dynamiclibrary_import"]
            qbs.install: true
            qbs.installDir: "/lib1"
        }
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

        Group {
            fileTagsFilter: ["dynamiclibrary", "dynamiclibrary_import"]
            qbs.install: true
            qbs.installDir: "/lib3"
        }
    }

    DynamicLibrary { // Non-dependency, referred to by name
        name: "lib4"
        Depends { name: "cpp" }

        files: ["lib4.cpp"]

    Properties {
            condition: qbs.targetOS.contains("darwin")
            bundle.isBundle: false
        }

        Group {
            fileTagsFilter: ["dynamiclibrary", "dynamiclibrary_import"]
            qbs.install: true
            qbs.installDir: "/lib4"
        }
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
        name: "app"
        consoleApplication: true
        files: "main.cpp"

        Depends { name: "lib1" }
        Depends { name: "lib2" }
        Depends { name: "lib3"; cpp.link: false }
        Depends { name: "lib4"; cpp.link: false }

        property string fullInstallPrefix: FileInfo.joinPaths(qbs.installRoot, qbs.installPrefix)
        property string lib3FilePath: FileInfo.joinPaths(fullInstallPrefix, "lib3",
                cpp.dynamicLibraryPrefix + "lib3" + (qbs.toolchain.contains("msvc")
                                                     ? ".lib" : cpp.dynamicLibrarySuffix))
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
