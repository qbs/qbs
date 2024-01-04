Project {
    DynamicLibrary {
        Depends { name: "bundle" }
        Depends { name: "cpp" }

        bundle.isBundle: false
        name: "DynamicLibraryA"
        files: ["rpathlink-deduplication-lib.cpp"]
    }

    DynamicLibrary {
        Depends { name: "bundle" }
        Depends { name: "cpp" }
        Depends { name: "DynamicLibraryA" }

        bundle.isBundle: false
        name: "DynamicLibraryB"
        files: ["rpathlink-deduplication-lib.cpp"]
    }

    DynamicLibrary {
        Depends { name: "bundle" }
        Depends { name: "cpp" }
        Depends { name: "DynamicLibraryA" }

        bundle.isBundle: false
        name: "DynamicLibraryC"
        files: ["rpathlink-deduplication-lib.cpp"]
    }

    CppApplication {
        Depends { name: "bundle" }
        Depends { name: "DynamicLibraryB" }
        Depends { name: "DynamicLibraryC" }
        consoleApplication: true
        bundle.isBundle: false
        cpp.removeDuplicateLibraries: false
        files: "rpathlink-deduplication-main.cpp"
        property bool test: {
            if (cpp.useRPathLink)
                console.info("useRPathLink: true");
            else
                console.info("useRPathLink: false");
            console.info("===" + cpp.rpathLinkFlag + "===");
        }
    }
}