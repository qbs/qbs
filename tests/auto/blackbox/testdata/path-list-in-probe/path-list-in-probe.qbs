Project {
  CppApplication {
    Probe {
        id: theProbe
        property pathList result
        configure: {
            result = ["main.cpp"]
            found = true
        }
    }
    property pathList res: theProbe.found ? theProbe.result : []
    consoleApplication: true
    Group {
        name: "files"
        files: res
    }
  }
}
