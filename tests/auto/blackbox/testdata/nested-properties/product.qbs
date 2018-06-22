Project {
    Product {
        name: "dep"
        Export {
            Depends { name: "lowerlevel" }
            lowerlevel.someOtherProp: "blubb"
        }
    }
    Product {
        type: "mytype"
        name: "toplevel"
        Depends { name: "higherlevel" }
        Depends { name: "lowerlevel" }
        Depends { name: "dep" }
        Group {
            files: ["dummy.txt"]
            fileTags: ["dummy-input"]
        }
    }
}
