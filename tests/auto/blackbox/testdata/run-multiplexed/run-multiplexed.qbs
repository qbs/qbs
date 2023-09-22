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
        configure: {
            if (targetPlatform !== Host.platform())
                console.info("targetPlatform differs from hostPlatform");
        }
    }
}
