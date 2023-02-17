Product {
    Depends { name: "cpp" }
    cpp.positionIndependentCode: false
    Properties {
        condition: qbs.toolchain.includes("iar")
            && qbs.architecture === "stm8"
        cpp.driverLinkerFlags: [
            "--config_def", "_CSTACK_SIZE=0x100",
            "--config_def", "_HEAP_SIZE=0x100",
        ]
    }
    Properties {
        condition: qbs.toolchain.includes("iar")
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
        condition: qbs.toolchain.includes("iar")
            && qbs.architecture === "rh850"
        cpp.driverLinkerFlags: [
            "--config_def", "CSTACK_SIZE=0x1000",
            "--config_def", "HEAP_SIZE=0x1000",
            "--config", cpp.toolchainInstallPath + "/../config/lnkrh850_g3m.icf"
        ]
    }
    Properties {
        condition: qbs.toolchain.includes("iar")
            && qbs.architecture === "v850"
        cpp.driverLinkerFlags: [
            "-D_CSTACK_SIZE=1000",
            "-D_HEAP_SIZE=1000",
            "-f", cpp.toolchainInstallPath + "/../config/lnk85.xcl"
        ]
    }
    Properties {
        condition: qbs.toolchain.includes("iar")
            && qbs.architecture === "78k"
        cpp.commonCompilerFlags: [
            "--core", "78k0",
            "--code_model", "standard"
        ]
        cpp.driverLinkerFlags: [
            "-D_CSTACK_SIZE=80",
            "-D_HEAP_SIZE=200",
            "-D_CODEBANK_START=0",
            "-D_CODEBANK_END=0",
            "-D_CODEBANK_BANKS=0",
            "-f", cpp.toolchainInstallPath + "/../config/lnk.xcl",
            cpp.toolchainInstallPath + "/../lib/clib/cl78ks1.r26"
        ]
    }
    Properties {
        condition: qbs.toolchain.includes("iar")
            && qbs.architecture === "sh"
        cpp.driverLinkerFlags: [
            "--config_def", "_CSTACK_SIZE=0x800",
            "--config_def", "_HEAP_SIZE=0x800",
            "--config_def", "_INT_TABLE=0x10",
            "--config", cpp.toolchainInstallPath + "/../config/generic.icf"
        ]
    }
    Properties {
        condition: qbs.toolchain.includes("iar")
            && qbs.architecture === "hcs8"
        cpp.driverLinkerFlags: [
            "-D_CSTACK_SIZE=200",
            "-D_HEAP_SIZE=200",
            "-f", cpp.toolchainInstallPath + "/../config/lnkunspecifieds08.xcl"
        ]
    }
    Properties {
        condition: qbs.toolchain.includes("iar")
            && qbs.architecture === "m32c"
        cpp.driverLinkerFlags: [
            "-D_CSTACK_SIZE=100",
            "-D_NEAR_HEAP_SIZE=400",
            "-D_FAR_HEAP_SIZE=400",
            "-D_HUGE_HEAP_SIZE=400",
            "-D_ISTACK_SIZE=40",
            "-f", cpp.toolchainInstallPath + "/../config/lnkm32c.xcl",
            cpp.toolchainInstallPath + (qbs.debugInformation ? "/../lib/dlib/dlm32cnf.r48" : "/../lib/clib/clm32cf.r48")
        ]
    }
    Properties {
        condition: qbs.toolchain.includes("iar")
            && qbs.architecture === "riscv"
        cpp.driverLinkerFlags: [
            "--config_def", "CSTACK_SIZE=0x1000",
            "--config_def", "HEAP_SIZE=0x1000"
        ]
    }
    Properties {
        condition: qbs.toolchain.includes("iar")
            && qbs.architecture === "m68k"
        cpp.cFlags: [
            "--no_div"
        ]
        cpp.driverLinkerFlags: [
            "-D__FLASHBEGIN=0",
            "-D__FLASHEND=1FFFF",
            "-D__RAMBEGIN=800000",
            "-D__RAMEND=803FFF",
            "-D_CSTACK_SIZE=200",
            "-D_HEAP_SIZE=1000",
            "-D_VBR_ADDRESS=0",
            "-f", cpp.toolchainInstallPath + "/../config/lnkm51ac128.xcl",
            cpp.toolchainInstallPath + "/../lib/dlcfcffdn.r68"
        ]
    }
    Properties {
        condition: qbs.toolchain.includes("keil")
            && qbs.architecture.startsWith("arm")
            && cpp.compilerName.startsWith("armcc")
        cpp.assemblerFlags: ["--cpu", "cortex-m0"]
        cpp.commonCompilerFlags: ["--cpu", "cortex-m0"]
    }
    Properties {
        condition: qbs.toolchain.includes("keil")
            && qbs.architecture.startsWith("arm")
            && cpp.compilerName.startsWith("armclang")
        cpp.assemblerFlags: ["--cpu", "cortex-m0"]
        cpp.commonCompilerFlags: ["-mcpu=cortex-m0", "--target=arm-arm-none-eabi"]
    }
    Properties {
        condition: qbs.toolchain.includes("gcc")
            && qbs.architecture.startsWith("arm")
        cpp.driverFlags: ["-specs=nosys.specs"]
    }
    Properties {
        condition: qbs.toolchain.includes("gcc")
            && qbs.architecture === "xtensa"
        cpp.driverFlags: ["-nostdlib"]
    }
    Properties {
        condition: qbs.toolchain.includes("gcc")
            && qbs.architecture === "msp430"
        cpp.driverFlags: ["-mmcu=msp430f5529", "-nostdlib"]
    }
    Properties {
        condition: qbs.toolchain.includes("gcc")
            && qbs.architecture === "m32r"
        cpp.driverFlags: ["-nostdlib"]
    }
    Properties {
        condition: qbs.toolchain.includes("gcc")
            && qbs.architecture === "riscv"
        cpp.driverFlags: ["-nostdlib"]
    }
}
