import qbs 1.0

Product {
    Depends { name: "cpp" }
    type: "application"
    consoleApplication: true
    files: ["zoo.cpp"]
}
