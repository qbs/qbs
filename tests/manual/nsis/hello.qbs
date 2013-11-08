import qbs

NSISSetup {
    name: "Qbs Hello"
    targetName: "qbs-hello-" + qbs.architecture
    files: ["hello.nsi", "hello.bat"]
    nsis.defines: ["batchFile=hello.bat"]
    nsis.compressor: "lzma-solid"
}
