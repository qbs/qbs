StaticLibrary {
    Properties {
        condition: isForDarwin
        bundle.isBundle: false
    }

    Depends { name: "cpp" }
    files: name + ".c"
    destinationDirectory: project.libDir
}
