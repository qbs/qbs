import qbs.Host

CppApplication {
    aggregate: false
    consoleApplication: true
    name: "app"
    multiplexByQbsProperties: "buildVariants"

    qbs.buildVariants: ["debug", "release"]

    files: "main.cpp"

    Probe {
        id: checker
        property string targetPlatform: qbs.targetPlatform
        property string targetArchitecture: qbs.architecture
        configure: {
            var result = targetPlatform === Host.platform() && targetArchitecture === Host.architecture();
            if (!result)
                console.info("target platform/arch differ from host platform/arch");
        }
    }
}
