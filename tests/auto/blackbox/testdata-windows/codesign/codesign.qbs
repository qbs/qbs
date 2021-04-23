Project {
    name: "p"

    property bool enableSigning: true
    property string hashAlgorithm
    property string subjectName
    property string signingTimestamp

    CppApplication {
        name: "A"
        files: "app.cpp"
        codesign.enableCodeSigning: project.enableSigning
        codesign.hashAlgorithm: project.hashAlgorithm
        codesign.subjectName: project.subjectName
        codesign.signingTimestamp: project.signingTimestamp
        install: true
        installDir: ""
        property bool dummy: {
            console.info("signtool path: %%" + codesign.codesignPath + "%%");
        }
    }

    DynamicLibrary {
        Depends { name: "cpp" }
        name: "B"
        files: "app.cpp"
        codesign.enableCodeSigning: project.enableSigning
        codesign.hashAlgorithm: project.hashAlgorithm
        codesign.subjectName: project.subjectName
        codesign.signingTimestamp: project.signingTimestamp
        install: true
        installDir: ""
        property bool dummy: {
            console.info("signtool path: %%" + codesign.codesignPath + "%%");
        }
    }
}
