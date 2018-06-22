Project {
    name: "subproject2"
    property string libNamePrefix: "test"
    SubProject {
        filePath: "subproject3/subproject3.qbs"
        inheritProperties: true
        Properties {
            name: "overridden name"
            condition: qbs.targetOS.length > 0
            libNameSuffix: "Lib"
        }
    }
}
