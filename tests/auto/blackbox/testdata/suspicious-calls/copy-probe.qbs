import qbs.File

Product {
    Probe {
        id: theProbe
        property string baseDir: project.sourceDirectory

        configure: {
            File.copy(baseDir + "/copy-probe.qbs",
                      baseDir + "/copy-probe2.qbs");
        }
    }
}
