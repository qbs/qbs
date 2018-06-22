import qbs.FileInfo

Project {
    property string installPrefix: "/usr"
    qbsSearchPaths: sourceDirectory
    Product {
        name: "local"
        Export {
            property bool dummy
            Depends { name: "cpp" }
            cpp.includePaths: ["/somelocaldir/include"]
        }
    }

    references: ["consumer.qbs", "lib.qbs", "tool.qbs"]
}
