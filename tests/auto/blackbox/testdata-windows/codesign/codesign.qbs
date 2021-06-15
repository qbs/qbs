Project {
    name: "p"

    property bool enableSigning: true
    property string hashAlgorithm
    property string subjectName
    property string signingTimestamp

    CppApplication {
        name: "A"
        files: "app.cpp"
        condition: qbs.toolchain.contains("msvc")
        codesign.enableCodeSigning: project.enableSigning
        codesign.hashAlgorithm: project.hashAlgorithm
        codesign.subjectName: project.subjectName
        codesign.signingTimestamp: project.signingTimestamp
        codesign.timestampAlgorithm: "sha256"
        install: true
        installDir: ""
        property bool dummy: {
            if (codesign.codesignPath)
                console.info("signtool path: %%" + codesign.codesignPath + "%%");
        }
    }

    DynamicLibrary {
        Depends { name: "cpp" }
        name: "B"
        files: "app.cpp"
        condition: qbs.toolchain.contains("msvc")
        codesign.enableCodeSigning: project.enableSigning
        codesign.hashAlgorithm: project.hashAlgorithm
        codesign.subjectName: project.subjectName
        codesign.signingTimestamp: project.signingTimestamp
        codesign.timestampAlgorithm: "sha256"
        install: true
        installDir: ""
        property bool dummy: {
            if (codesign.codesignPath)
                console.info("signtool path: %%" + codesign.codesignPath + "%%");
        }
    }
}
