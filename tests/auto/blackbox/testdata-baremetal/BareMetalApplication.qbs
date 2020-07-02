CppApplication {
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
    Properties {
        condition: qbs.toolchain.contains("gcc")
            && qbs.architecture.startsWith("arm")
        cpp.driverFlags: ["-specs=nosys.specs"]
    }
    Properties {
        condition: qbs.toolchain.contains("gcc")
            && qbs.architecture.startsWith("xtensa")
        cpp.driverFlags: ["-nostdlib"]
    }
    Properties {
        condition: qbs.toolchain.contains("gcc")
            && qbs.architecture === "msp430"
        cpp.driverFlags: ["-mmcu=msp430f5529"]
    }
}
