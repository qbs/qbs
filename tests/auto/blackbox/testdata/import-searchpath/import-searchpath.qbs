Project {
    Project {
        qbsSearchPaths: ["qbs"]
        references: ["src/import-searchpath-app1.qbs"]
    }
    Project {
        references: ["src/import-searchpath-app2.qbs"]
    }
}
