NSISSetup {
    name: "Qbs Hello"
    targetName: "qbs-hello-" + qbs.architecture

    property bool dummy: { console.info("is Windows: " + qbs.targetOS.includes("windows")); }

    Properties {
        condition: nsis.present
        nsis.defines: ["batchFile=hello.bat"]
        nsis.compressor: "lzma-solid"
    }

    files: ["hello.nsi", "hello.bat"]
}
