Project {
    name: "top level project"
    references: ["subproject2"]

    Project {
        condition: true
        name: "app-project"
        CppApplication {
            name: "app"
            Depends { name: "testLib" }
            cpp.defines: "MY_EXPORT="
            files: "subproject1/main.cpp"
        }
    }

    qbsSearchPaths: ["resources"]
}
