Project {
    condition: {
        if (qbs.targetPlatform === "windows" && qbs.architecture === "x86") {
            if (qbs.toolchainType === "watcom")
                return true;
            if (qbs.toolchainType === "dmc")
                return true;
        }

        console.info("unsupported toolset: %%"
            + qbs.toolchainType + "%%, %%" + qbs.architecture + "%%");
        return false;
    }

    property bool enableSigning: true
    property string hashAlgorithm
    property string subjectName
    property string signingTimestamp

    CppApplication {
        name: "A"
        files: "app.c"
        consoleApplication: true
        codesign.enableCodeSigning: project.enableSigning
        codesign.hashAlgorithm: project.hashAlgorithm
        codesign.subjectName: project.subjectName
        codesign.signingTimestamp: project.signingTimestamp
        codesign.timestampAlgorithm: "sha256"
        property bool dummy: {
            if (codesign.codesignPath)
                console.info("signtool path: %%" + codesign.codesignPath + "%%");
        }
    }

    DynamicLibrary {
        Depends { name: "cpp" }
        name: "B"
        files: "lib.c"
        codesign.enableCodeSigning: project.enableSigning
        codesign.hashAlgorithm: project.hashAlgorithm
        codesign.subjectName: project.subjectName
        codesign.signingTimestamp: project.signingTimestamp
        codesign.timestampAlgorithm: "sha256"
        property bool dummy: {
            if (codesign.codesignPath)
                console.info("signtool path: %%" + codesign.codesignPath + "%%");
        }
    }
}
