import qbs
import qbs.File
import qbs.FileInfo
import qbs.TextFile

Module {
    Probe {
        id: qbsVersionProbe
        property var lastModified: File.lastModified(versionFilePath)
        property string versionFilePath: FileInfo.joinPaths(path, "..", "..", "..", "VERSION")
        property string version
        configure: {
            var tf = new TextFile(versionFilePath, TextFile.ReadOnly);
            try {
                version = tf.readAll().trim();
                found = !!version;
            } finally {
                tf.close();
            }
        }
    }

    version: qbsVersionProbe.version
}
