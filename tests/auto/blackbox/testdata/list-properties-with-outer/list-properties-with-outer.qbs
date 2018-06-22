Product {
    type: ["outtype"]
    Depends { name: "higher" }
    lower.listProp: ["product"]
    Group {
        files: ["dummy.txt"]
        fileTags: ["intype"]
        lower.listProp: outer.concat(["group"])
    }
}
