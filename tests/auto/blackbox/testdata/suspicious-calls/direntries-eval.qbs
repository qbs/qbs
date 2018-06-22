import qbs.File

Product {
    name: File.directoryEntries(sourceDirectory, File.Files)[0]
}
