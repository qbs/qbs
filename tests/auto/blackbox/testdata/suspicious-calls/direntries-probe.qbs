import qbs.File

Product {
    Probe {
        id: theProbe
        property string baseDir: project.sourceDirectory
        property stringList subDirs

        configure: {
            subDirs = File.directoryEntries(baseDir, File.AllDirs);
            found = true;
        }
    }
}
