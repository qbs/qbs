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
        condition: qbs.toolchain.contains("iar")
            && qbs.architecture === "rl78"
        cpp.driverLinkerFlags: [
            "--config_def", "_NEAR_HEAP_SIZE=256",
            "--config_def", "_FAR_HEAP_SIZE=4096",
            "--config_def", "_HUGE_HEAP_SIZE=0",
            "--config_def", "_STACK_SIZE=128",
            "--config_def", "_NEAR_CONST_LOCATION_SIZE=0x6F00",
            "--config_def", "_NEAR_CONST_LOCATION_START=0x3000",
            "--define_symbol", "_NEAR_CONST_LOCATION=0",
            "--config", cpp.toolchainInstallPath + "/../config/lnkrl78_s3.icf"
        ]
    }
    Properties {
        condition: qbs.toolchain.contains("iar")
            && qbs.architecture === "rh850"
        cpp.driverLinkerFlags: [
            "--config_def", "CSTACK_SIZE=0x1000",
            "--config_def", "HEAP_SIZE=0x1000",
            "--config", cpp.toolchainInstallPath + "/../config/lnkrh850_g3m.icf"
        ]
    }
    Properties {
        condition: qbs.toolchain.contains("iar")
            && qbs.architecture === "v850"
        cpp.driverLinkerFlags: [
            "-D_CSTACK_SIZE=1000",
            "-D_HEAP_SIZE=1000",
            "-f", cpp.toolchainInstallPath + "/../config/lnk85.xcl"
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
