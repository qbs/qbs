CppApplication {
    Properties {
        condition: qbs.toolchain.contains("keil") && qbs.architecture.startsWith("arm")
        cpp.driverFlags: ["--cpu", "cortex-m0"]
    }
}
