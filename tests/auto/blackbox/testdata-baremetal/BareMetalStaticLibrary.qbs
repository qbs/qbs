StaticLibrary {
    Properties {
        condition: qbs.toolchain.contains("iar")
            && qbs.architecture === "stm8"
        cpp.driverLinkerFlags: [
            "--config_def", "_CSTACK_SIZE=0x100",
            "--config_def", "_HEAP_SIZE=0x100",
        ]
    }
    Properties {
        condition: qbs.toolchain.contains("keil")
            && qbs.architecture.startsWith("arm")
            && cpp.compilerName.startsWith("armcc")
        cpp.assemblerFlags: ["--cpu", "cortex-m0"]
        cpp.driverFlags: ["--cpu", "cortex-m0"]
    }
    Properties {
        condition: qbs.toolchain.contains("keil")
            && qbs.architecture.startsWith("arm")
            && cpp.compilerName.startsWith("armclang")
        cpp.assemblerFlags: ["--cpu", "cortex-m0"]
        cpp.driverFlags: ["-mcpu=cortex-m0", "--target=arm-arm-none-eabi"]
    }
    Properties {
        condition: qbs.toolchain.contains("gcc")
            && qbs.architecture.startsWith("arm")
        cpp.driverFlags: ["-specs=nosys.specs"]
    }
    Properties {
        condition: qbs.toolchain.contains("gcc")
            && qbs.architecture === "xtensa"
        cpp.driverFlags: ["-nostdlib"]
    }
    Properties {
        condition: qbs.toolchain.contains("gcc")
            && qbs.architecture === "msp430"
        cpp.driverFlags: ["-mmcu=msp430f5529"]
    }
}
