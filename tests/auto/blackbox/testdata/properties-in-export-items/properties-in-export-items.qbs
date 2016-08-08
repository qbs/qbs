import qbs

Project {
    Product {
        name: "dep"
        Export {
            property string theDefine
            Depends { name: "cpp" }
            cpp.defines: [theDefine]
        }
    }

    Application {
        name: "p1"
        Depends { name: "dep" }
        dep.theDefine: "P1"
        files: ["main1.cpp"]
    }
    Application {
        name: "p2"
        Depends { name: "dep" }
        Group {
            files: ["main2.cpp"]
        }
        dep.theDefine: "P2" // TODO: Move to Group when QBS-1005 is fixed.
    }
}
