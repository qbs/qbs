import qbs.Utilities

// no minimum versions are specified so the profile defaults will be used
import qbs.Host

CppApplication {
    condition: {
        var result = qbs.targetPlatform === Host.platform();
        if (!result)
            console.info("targetPlatform differs from hostPlatform");
        return result;
    }
    files: [qbs.targetOS.includes("darwin") ? "main.mm" : "main.cpp"]
    consoleApplication: true

    Properties {
        condition: qbs.targetOS.includes("windows")
        cpp.defines: ["TOOLCHAIN_INSTALL_PATH=" + Utilities.cStringQuote(cpp.toolchainInstallPath)]
    }

    Properties {
        condition: qbs.targetOS.includes("darwin")
        cpp.frameworks: "Foundation"
    }
}
