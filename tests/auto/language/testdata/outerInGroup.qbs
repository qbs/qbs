Project {
    Product {
        name: "OuterInGroup"
        Depends { name: "dummy" }
        qbs.installDir: "/somewhere"
        dummy.someString: "s1"
        Properties { dummy.someString: outer.concat("s2") }
        files: ["main.cpp"]
        Group {
            Group {
                name: "Special Group"
                files: ["aboutdialog.cpp"]
                qbs.installDir: outer + "/else"
                dummy.someString: outer.concat("s3")
            }
        }
    }
}
