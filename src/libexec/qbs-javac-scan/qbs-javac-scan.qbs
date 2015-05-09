import qbs

JavaJarFile {
    condition: project.enableJava

    property string entryPoint: "io.qt.qbs.tools.JavaCompilerScannerTool"

    destinationDirectory: "libexec"

    Group {
        fileTagsFilter: ["java.jar"]
        qbs.install: true
        qbs.installDir: "libexec"
    }

    files: [
        "io/qt/qbs/**/*.java"
    ]
}
