Project {
    SubProject { // This strange construct tests QBS-1836
        inheritProperties: false
        filePath: "subproject.qbs"
    }
}
