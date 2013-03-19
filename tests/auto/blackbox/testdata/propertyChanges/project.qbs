import qbs 1.0

Project {
    property var projectDefines: ["blubb2"]
    CppApplication {
        name: "product 1"
        cpp.defines: ["blubb1"]
        files: "source1.cpp"
    }
    CppApplication {
        name: "product 2"
        cpp.defines: project.projectDefines
        files: "source2.cpp"
    }
}
