import qbs

Project {
    name: "subproject2"
    property string libNamePrefix: "test"
    SubProject {
        filePath: "subproject3/subproject3.qbs"
        inheritProperties: true
        Properties {
            name: "overridden name"
            condition: true
            libNameSuffix: "Lib"
        }
    }
}
