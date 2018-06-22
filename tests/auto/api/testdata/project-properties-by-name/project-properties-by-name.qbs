Project {
    name: "toplevel"
    property stringList theDefines: []
    Project {
        name: "subproject1"
        CppApplication {
            name: "subproduct1"
            files: ["main1.cpp"]
            cpp.defines: project.theDefines
        }
    }
    Project {
        name: "subproject2"
        CppApplication {
            name: "subproduct2"
            files: ["main2.cpp"]
            cpp.defines: project.theDefines
        }
    }
}
