import qbs

Project {
    SubProject {
        Properties { condition: false }
        Properties { name: "blubb" }
    }
}
