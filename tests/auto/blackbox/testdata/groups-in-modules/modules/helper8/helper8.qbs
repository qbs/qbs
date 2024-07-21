Module {
    version: "8"
    Probe {
        id: baseNameProbe
        property string baseName
        configure: { baseName = "helper" }
    }
    property string fileName: baseNameProbe.baseName + version + ".c"
    Group {
        condition: version === "8"
        prefix: path + "/"
        files: fileName
    }
}
