Product {
    type: ["out1", "out2"]
    Depends { name: "m" }
    Group {
        files: ["in1.txt"]
        fileTags: ["in1"]
    }
    Group {
        files: ["in2.txt"]
        fileTags: ["in2"]
    }
}
