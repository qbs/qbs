import qbs.base 1.0

Project {
    Product {
        name: "OuterInGroup"
        qbs.installPrefix: "/somewhere"
        files: ["main.cpp"]
        Group {
            name: "Special Group"
            files: ["aboutdialog.cpp"]
            qbs.installPrefix: outer + "/else"
        }
    }
}
