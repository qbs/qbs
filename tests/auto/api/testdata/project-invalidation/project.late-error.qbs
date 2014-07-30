import qbs

Product {
    type: "mytype"

    Transformer {
        Artifact {
            filePath: "blubb"
            fileTags: "mytype"
        }
        prepare: []
    }
}
