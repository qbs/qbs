Project {
    minimumQbsVersion: "1.6"

    Product {
        name: "dep"
        Export {
            property string theDefine: ""
            Depends { name: "cpp" }
            cpp.defines: [theDefine]
        }
    }

    Application {
        name: "p1"
        consoleApplication: true
        Depends { name: "dep" }
        dep.theDefine: "P1"
        cpp.minimumMacosVersion: "10.9"
        files: ["main1.cpp"]
    }
    Application {
        name: "p2"
        consoleApplication: true
        Depends { name: "dep" }
        cpp.minimumMacosVersion: "10.9"
        Group {
            dep.theDefine: "P2"
            files: ["main2.cpp"]
        }
    }
}
