Project {
    Product {
        name: "OuterInGroup"
        qbs.installDir: "/somewhere"
        files: ["main.cpp"]
        Group {
            name: "Special Group"
            files: ["aboutdialog.cpp"]
            qbs.installDir: outer + "/else"
        }
    }
}
