Project {
    SubProject {
        condition: false
        filePath: "nosuchfile.qbs"
    }
    SubProject {
        Properties {
            condition: false
        }
        filePath: "nosuchfile.qbs"
    }
    SubProject {
        condition: true
        Properties {
            condition: false
        }
        filePath: "nosuchfile.qbs"
    }
    SubProject {
        condition: false
        Properties {
            condition: true
        }
        filePath: "nosuchfile.qbs"
    }
}
