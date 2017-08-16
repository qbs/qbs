import qbs
import qbs.Utilities

// no minimum versions are specified so the profile defaults will be used
CppApplication {
    files: [qbs.targetOS.contains("darwin") ? "main.mm" : "main.cpp"]
    consoleApplication: true

    Properties {
        condition: qbs.targetOS.contains("windows")
        cpp.defines: ["TOOLCHAIN_INSTALL_PATH=" + Utilities.cStringQuote(cpp.toolchainInstallPath)]
    }

    Properties {
        condition: qbs.targetOS.contains("darwin")
        cpp.frameworks: "Foundation"
    }
}
