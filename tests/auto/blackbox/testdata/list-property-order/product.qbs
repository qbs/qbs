Product {
    type: "outtype"
    name: "toplevel"
    Depends { name: "higher1" }
    Depends { name: "higher2" }
    Depends { name: "higher3" }
    lower.listProp: ["product"]
    Group {
        files: ["dummy.txt"]
        fileTags: ["intype"]
    }
}
