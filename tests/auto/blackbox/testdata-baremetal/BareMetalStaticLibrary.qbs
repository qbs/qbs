StaticLibrary {
    Properties {
        condition: qbs.toolchain.contains("keil")
            && qbs.architecture.startsWith("arm")
            && cpp.compilerName.startsWith("armcc")
        cpp.driverFlags: ["--cpu", "cortex-m0"]
    }
    Properties {
        condition: qbs.toolchain.contains("keil")
            && qbs.architecture.startsWith("arm")
            && cpp.compilerName.startsWith("armclang")
        cpp.driverFlags: ["-mcpu=cortex-m0", "--target=arm-arm-none-eabi"]
    }
}
