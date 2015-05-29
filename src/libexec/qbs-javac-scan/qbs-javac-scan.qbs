import qbs

JavaJarFile {
    condition: project.enableJava

    property string entryPoint: "io.qt.qbs.tools.JavaCompilerScannerTool"

    destinationDirectory: project.libexecInstallDir

    Group {
        fileTagsFilter: ["java.jar"]
        qbs.install: true
        qbs.installDir: project.libexecInstallDir
    }

    files: [
        "io/qt/qbs/**/*.java"
    ]
}
