import qbs 1.0

Product {
    Depends { name: "cpp" }
    type: "application"
    files: ["zoo.cpp"]
}
