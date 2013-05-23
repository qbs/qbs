import qbs 1.0

Project {
    property bool linkSuccessfully: false
    references: linkSuccessfully ? ["helper_lib.qbs"] : []
    CppApplication {
        type: "application"
        Depends {
            condition: project.linkSuccessfully
            name: "helperLib"
        }
        files: "main2.cpp"
    }
}
