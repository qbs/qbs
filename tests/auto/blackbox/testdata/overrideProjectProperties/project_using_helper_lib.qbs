Project {
    property bool linkSuccessfully: false
    references: linkSuccessfully ? ["helper_lib.qbs"] : []
    CppApplication {
        consoleApplication: true
        Depends {
            condition: project.linkSuccessfully
            name: "helperLib"
        }
        files: "main2.cpp"
    }
}
