import qbs
import qbs.File

Product {
    Probe {
        property string baseDir: project.sourceDirectory
        property stringList subDirs

        configure: {
            subDirs = File.directoryEntries(baseDir, File.AllDirs);
            found = true;
        }
    }
}
