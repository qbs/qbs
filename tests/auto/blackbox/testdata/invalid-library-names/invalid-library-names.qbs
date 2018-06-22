Project {
    minimumQbsVersion: "1.6"
    property var values: [null, undefined, 5, [], ""]
    property int valueIndex
    CppApplication {
        cpp.dynamicLibraries: [project.values[project.valueIndex]]
        cpp.staticLibraries: [project.values[project.valueIndex]]
        files: ["main.cpp"]
    }
}
