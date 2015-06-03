import qbs

Product {
    type: "outtype"
    name: "toplevel"
    Depends { name: "higher1" }
    Depends { name: "higher2" }
    Depends { name: "higher3" }
    Group {
        files: ["dummy.txt"]
        fileTags: ["intype"]
    }
}
