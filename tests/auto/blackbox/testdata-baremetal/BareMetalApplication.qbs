import qbs.Host

BareMetalProduct {
    type: "application"
    consoleApplication: true

    property bool dummy: {
        if (qbs.targetPlatform !== Host.platform()
                || qbs.architecture !== Host.architecture()) {

            function supportsCrossRun() {
                // We can run 32 bit applications on 64 bit Windows.
                if (Host.platform() === "windows" && Host.architecture() === "x86_64"
                        && qbs.targetPlatform === "windows" && qbs.architecture === "x86") {
                    return true;
                }
            }

            if (!supportsCrossRun())
                console.info("target platform/arch differ from host platform/arch")
        }
    }

    Group {
        condition: qbs.toolchain.includes("cosmic")
        files: "cosmic.lkf"
        fileTags: "linkerscript"
    }
}
