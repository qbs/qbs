import qbs
import qbs.File

Product {
    Probe {
        property string baseDir: project.sourceDirectory

        configure: {
            File.copy(baseDir + "/copy-probe.qbs",
                      baseDir + "/copy-probe2.qbs");
        }
    }
}
